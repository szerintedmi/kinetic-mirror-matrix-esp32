"""Wrapper for PlatformIO build, espota.py upload, and HTTP OTA upload commands."""

from __future__ import annotations

import asyncio
import json
import re
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import TYPE_CHECKING, AsyncIterator

if TYPE_CHECKING:
    from asyncio.subprocess import Process


@dataclass
class FirmwareInfo:
    """Information about the built firmware."""

    path: Path
    version: str  # GIT_COMMIT_HASH from version.h
    date: str  # GIT_COMMIT_DATE from version.h


OTA_PORT = 3232


class PioWrapper:
    """Wrapper for PlatformIO build and espota.py upload operations."""

    def __init__(self, env: str, project_dir: Path | None = None, ota_password: str = ""):
        self.env = env
        self.project_dir = project_dir or Path.cwd()
        self.ota_password = ota_password
        # espota.py progress pattern: "Uploading: [====     ] 45%"
        self.progress_pattern = re.compile(r"]\s*(\d+)%")

    async def build(self, log_file: Path | None = None) -> FirmwareInfo:
        """Build firmware and return info.

        Args:
            log_file: Optional path to write build output

        Returns:
            FirmwareInfo with path, version, and date
        """
        cmd = ["pio", "run", "-e", self.env]
        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
            cwd=self.project_dir,
        )

        output = await self._capture_output(proc, log_file, cmd)

        if proc.returncode != 0:
            raise RuntimeError(f"Build failed with code {proc.returncode}\n{output}")

        return await self.get_firmware_info()

    async def get_firmware_info(self) -> FirmwareInfo:
        """Extract firmware version and date from built artifacts."""
        firmware_path = self.project_dir / ".pio" / "build" / self.env / "firmware.bin"
        build_info_path = (
            self.project_dir / ".pio" / "build" / self.env / "firmware_build_info.json"
        )

        if not firmware_path.exists():
            raise FileNotFoundError(
                f"Firmware not found: {firmware_path}\nRun 'pio run -e {self.env}' to build first."
            )

        if not build_info_path.exists():
            raise FileNotFoundError(
                f"Build info not found: {build_info_path}\n"
                f"Run 'pio run -e {self.env}' to build first."
            )

        with open(build_info_path) as f:
            data = json.load(f)

        return FirmwareInfo(
            path=firmware_path,
            version=data["version"],
            date=data["date"],
        )

    async def build_filesystem(self, log_file: Path | None = None) -> Path:
        """Build filesystem image.

        Args:
            log_file: Optional path to write build output

        Returns:
            Path to littlefs.bin
        """
        cmd = ["pio", "run", "-e", self.env, "-t", "buildfs"]
        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
            cwd=self.project_dir,
        )

        output = await self._capture_output(proc, log_file, cmd)

        if proc.returncode != 0:
            raise RuntimeError(f"Filesystem build failed with code {proc.returncode}\n{output}")

        return self.get_filesystem_path()

    def get_filesystem_path(self) -> Path:
        """Get path to built littlefs.bin."""
        fs_path = self.project_dir / ".pio" / "build" / self.env / "littlefs.bin"
        if not fs_path.exists():
            raise FileNotFoundError(
                f"Filesystem image not found: {fs_path}\n"
                f"Run 'pio run -e {self.env} -t buildfs' to build first."
            )
        return fs_path

    def _find_espota(self) -> Path:
        """Find espota.py in PlatformIO packages."""
        espota_path = (
            self.project_dir
            / ".pio"
            / "packages"
            / "framework-arduinoespressif32"
            / "tools"
            / "espota.py"
        )
        if not espota_path.exists():
            raise FileNotFoundError(
                f"espota.py not found at {espota_path}. "
                "Run 'pio run -e esp32DedicatedStep' first to install packages."
            )
        return espota_path

    async def upload(self, ip: str, log_file: Path, firmware_path: Path) -> AsyncIterator[int]:
        """Upload firmware via OTA using espota.py directly.

        This bypasses PlatformIO's build system to avoid rebuilding,
        which prevents version.h regeneration and parallel build races.

        Args:
            ip: Device IP address
            log_file: Path to write upload output
            firmware_path: Path to firmware.bin file

        Yields:
            Progress percentage (0-100)
        """
        espota = self._find_espota()

        cmd = [
            "python3",
            str(espota),
            "-r",  # show progress bar output
            "-d",  # debug output
            "-i",
            ip,
            "-p",
            str(OTA_PORT),
            "-a",
            self.ota_password,
            "-f",
            str(firmware_path),
        ]

        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
            cwd=self.project_dir,
        )

        last_percent = 0
        with open(log_file, "wb") as log:
            # Write command header
            header = f"# Command: {' '.join(cmd)}\n# Started: {datetime.now().isoformat()}\n\n"
            log.write(header.encode())
            log.flush()

            assert proc.stdout is not None
            buffer = ""
            while True:
                # Read chunks instead of lines - espota.py uses \r for progress
                chunk = await proc.stdout.read(256)
                if not chunk:
                    break

                log.write(chunk)
                log.flush()

                # Decode and accumulate for pattern matching
                buffer += chunk.decode("utf-8", errors="ignore")

                # Parse progress percentage from "Uploading: [====     ] 45%"
                # Look for all matches in buffer
                for match in self.progress_pattern.finditer(buffer):
                    percent = int(match.group(1))
                    if percent > last_percent:
                        last_percent = percent
                        yield percent

                # Keep only recent buffer to avoid memory growth
                if len(buffer) > 1024:
                    buffer = buffer[-512:]

        await proc.wait()
        if proc.returncode != 0:
            detail = self._extract_error(buffer)
            msg = f"Upload failed (exit {proc.returncode})"
            if detail:
                msg += f": {detail}"
            msg += f"\nLog: {log_file}"
            raise RuntimeError(msg)

        yield 100

    async def upload_filesystem(self, ip: str, log_file: Path, fs_path: Path) -> AsyncIterator[int]:
        """Upload filesystem via OTA using espota.py with -s flag.

        Args:
            ip: Device IP address
            log_file: Path to write upload output
            fs_path: Path to littlefs.bin file

        Yields:
            Progress percentage (0-100)
        """
        espota = self._find_espota()

        cmd = [
            "python3",
            str(espota),
            "-r",  # show progress bar output
            "-d",  # debug output
            "-s",  # SPIFFS/LittleFS mode
            "-i",
            ip,
            "-p",
            str(OTA_PORT),
            "-a",
            self.ota_password,
            "-f",
            str(fs_path),
        ]

        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
            cwd=self.project_dir,
        )

        last_percent = 0
        with open(log_file, "wb") as log:
            header = f"# Command: {' '.join(cmd)}\n# Started: {datetime.now().isoformat()}\n\n"
            log.write(header.encode())
            log.flush()

            assert proc.stdout is not None
            buffer = ""
            while True:
                chunk = await proc.stdout.read(256)
                if not chunk:
                    break

                log.write(chunk)
                log.flush()

                buffer += chunk.decode("utf-8", errors="ignore")

                for match in self.progress_pattern.finditer(buffer):
                    percent = int(match.group(1))
                    if percent > last_percent:
                        last_percent = percent
                        yield percent

                if len(buffer) > 1024:
                    buffer = buffer[-512:]

        await proc.wait()
        if proc.returncode != 0:
            detail = self._extract_error(buffer)
            msg = f"Filesystem upload failed (exit {proc.returncode})"
            if detail:
                msg += f": {detail}"
            msg += f"\nLog: {log_file}"
            raise RuntimeError(msg)

        yield 100

    async def upload_http(self, ip: str, log_file: Path, firmware_path: Path) -> AsyncIterator[int]:
        """Upload firmware via HTTP POST to /api/ota endpoint.

        Args:
            ip: Device IP address
            log_file: Path to write upload output
            firmware_path: Path to firmware.bin file

        Yields:
            Progress percentage (0-100)
        """
        async for percent in self._upload_http_impl(ip, log_file, firmware_path, "firmware"):
            yield percent

    async def upload_filesystem_http(
        self, ip: str, log_file: Path, fs_path: Path
    ) -> AsyncIterator[int]:
        """Upload filesystem via HTTP POST to /api/ota?type=filesystem endpoint.

        Args:
            ip: Device IP address
            log_file: Path to write upload output
            fs_path: Path to littlefs.bin file

        Yields:
            Progress percentage (0-100)
        """
        async for percent in self._upload_http_impl(ip, log_file, fs_path, "filesystem"):
            yield percent

    async def _upload_http_impl(
        self, ip: str, log_file: Path, image_path: Path, upload_type: str
    ) -> AsyncIterator[int]:
        """Shared HTTP upload implementation for firmware and filesystem.

        Streams the body in chunks and reports upload progress via a background
        task so the caller sees real-time percentage updates.

        Yields:
            Progress percentage (0-100)
        """
        import aiohttp

        url = f"http://{ip}/api/ota?type={upload_type}"
        firmware_data = image_path.read_bytes()
        file_size = len(firmware_data)
        chunk_size = 16 * 1024

        # Shared progress counter read by the outer generator
        bytes_sent = 0

        async def body_generator() -> AsyncIterator[bytes]:
            nonlocal bytes_sent
            for offset in range(0, file_size, chunk_size):
                chunk = firmware_data[offset : offset + chunk_size]
                bytes_sent += len(chunk)
                yield chunk

        headers = {
            "X-OTA-Password": self.ota_password,
            "Content-Type": "application/octet-stream",
            "Content-Length": str(file_size),
        }

        log_handle = open(log_file, "w")
        log_handle.write(f"# HTTP POST {url}\n")
        log_handle.write(f"# Started: {datetime.now().isoformat()}\n")
        log_handle.write(f"# File: {image_path} ({file_size} bytes)\n\n")
        log_handle.flush()

        # Result container populated by the background upload task
        upload_result: dict[str, object] = {}

        async def do_upload() -> None:
            try:
                timeout = aiohttp.ClientTimeout(total=120, sock_connect=10)
                async with aiohttp.ClientSession(timeout=timeout) as session:
                    async with session.post(url, headers=headers, data=body_generator()) as resp:
                        body = await resp.text()
                        log_handle.write(f"HTTP {resp.status}\n{body}\n")
                        log_handle.flush()
                        upload_result["status"] = resp.status
                        upload_result["body"] = body
            except aiohttp.ClientError as e:
                log_handle.write(f"\nConnection error: {e}\n")
                log_handle.flush()
                upload_result["error"] = e

        task = asyncio.create_task(do_upload())

        last_percent = 0
        try:
            while not task.done():
                await asyncio.sleep(0.25)
                if file_size > 0:
                    percent = min((bytes_sent * 100) // file_size, 99)
                    if percent > last_percent:
                        last_percent = percent
                        yield percent

            # Propagate any unhandled exceptions from the task
            task.result()

            if "error" in upload_result:
                raise RuntimeError(f"Upload failed: {upload_result['error']}\nLog: {log_file}")

            status = upload_result["status"]
            body = upload_result["body"]
            if status != 200:
                error_msg = str(body)
                try:
                    data = json.loads(str(body))
                    error_msg = data.get("message", str(body))
                except (json.JSONDecodeError, KeyError):
                    pass
                raise RuntimeError(f"Upload failed (HTTP {status}): {error_msg}\nLog: {log_file}")
        finally:
            log_handle.close()

        yield 100

    @staticmethod
    def _extract_error(buffer: str) -> str:
        """Extract last meaningful error line from espota.py output buffer."""
        for line in reversed(buffer.splitlines()):
            line = line.strip()
            # Skip empty lines and progress bar output
            if not line or ("] " in line and "%" in line):
                continue
            return line
        return ""

    async def _capture_output(
        self, proc: Process, log_file: Path | None, cmd: list[str] | None = None
    ) -> str:
        """Capture process output, optionally writing to log file."""
        assert proc.stdout is not None
        output_lines: list[bytes] = []

        async def read_lines(log_handle: object | None) -> None:
            while True:
                line = await proc.stdout.readline()  # type: ignore[union-attr]
                if not line:
                    break
                output_lines.append(line)
                if log_handle:
                    log_handle.write(line)  # type: ignore[union-attr]
                    log_handle.flush()  # type: ignore[union-attr]

        if log_file:
            with open(log_file, "wb") as log_handle:
                # Write command header
                if cmd:
                    header = (
                        f"# Command: {' '.join(cmd)}\n# Started: {datetime.now().isoformat()}\n\n"
                    )
                    log_handle.write(header.encode())
                    log_handle.flush()
                await read_lines(log_handle)
        else:
            await read_lines(None)

        await proc.wait()
        return b"".join(output_lines).decode("utf-8", errors="ignore")
