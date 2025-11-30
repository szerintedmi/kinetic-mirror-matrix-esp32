"""Device verification via HTTP /api/status endpoint."""

from __future__ import annotations

import asyncio
import json
from dataclasses import dataclass


@dataclass
class VerificationResult:
    """Result of device firmware verification."""

    verified: bool
    firmware_version: str | None = None
    firmware_date: str | None = None
    error: str | None = None


async def verify_device(
    ip: str,
    expected_version: str,
    expected_date: str,
    timeout: float = 5.0,
) -> VerificationResult:
    """Verify device firmware via GET /api/status.

    Waits for device to reboot after OTA upload, then checks if the
    firmware version and date match expected values.

    Args:
        ip: Device IP address
        expected_version: Expected firmware version (GIT_COMMIT_HASH)
        expected_date: Expected firmware date (GIT_COMMIT_DATE)
        timeout: HTTP request timeout in seconds

    Returns:
        VerificationResult with verification status and details
    """
    # Wait a bit for device to reboot after OTA
    await asyncio.sleep(3.0)

    try:
        # Use curl subprocess (zero new dependencies)
        proc = await asyncio.create_subprocess_exec(
            "curl",
            "-s",
            "--max-time",
            str(timeout),
            f"http://{ip}/api/status",
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )

        stdout, stderr = await proc.communicate()

        if proc.returncode != 0:
            return VerificationResult(
                verified=False,
                error=f"HTTP request failed: {stderr.decode().strip() or 'connection error'}",
            )

        data = json.loads(stdout.decode())
        fw_version = data.get("firmwareVersion", "")
        fw_date = data.get("firmwareDate", "")

        if fw_version != expected_version:
            return VerificationResult(
                verified=False,
                firmware_version=fw_version,
                firmware_date=fw_date,
                error=f"Version mismatch: got {fw_version}, expected {expected_version}",
            )

        if fw_date != expected_date:
            return VerificationResult(
                verified=False,
                firmware_version=fw_version,
                firmware_date=fw_date,
                error=f"Date mismatch: got {fw_date}, expected {expected_date}",
            )

        return VerificationResult(
            verified=True,
            firmware_version=fw_version,
            firmware_date=fw_date,
        )

    except json.JSONDecodeError as e:
        return VerificationResult(
            verified=False,
            error=f"Invalid JSON response: {e}",
        )
    except Exception as e:
        return VerificationResult(
            verified=False,
            error=f"Verification failed: {e}",
        )
