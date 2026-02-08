#!/usr/bin/env python3
"""OTA deployment CLI for ESP32 devices.

Builds and deploys firmware and/or filesystem to devices via OTA.
Verifies successful firmware deployment by checking /api/status on each device.

Usage:
    # Deploy firmware to single device
    poetry run python -m tools.deploy.ota_deploy --device 192.168.1.100

    # Deploy firmware to all devices in config (default)
    poetry run python -m tools.deploy.ota_deploy

    # Deploy filesystem only
    poetry run python -m tools.deploy.ota_deploy --filesystem --device <IP>

    # Deploy both firmware and filesystem
    poetry run python -m tools.deploy.ota_deploy --with-filesystem --device <IP>

    # Build only (no deploy)
    poetry run python -m tools.deploy.ota_deploy --build-only

    # Deploy existing build (skip build step)
    poetry run python -m tools.deploy.ota_deploy --skip-build

    # Retry failed devices from previous run
    poetry run python -m tools.deploy.ota_deploy --retry tools/deploy/logs/<timestamp>_summary.json
"""

from __future__ import annotations

import argparse
import asyncio
import json
import sys
from dataclasses import asdict, dataclass, field
from datetime import datetime
from pathlib import Path

import tomllib
from rich.console import Console
from rich.live import Live
from rich.progress import BarColumn, Progress, SpinnerColumn, TaskID, TextColumn
from rich.table import Table

from tools.deploy.pio_wrapper import FirmwareInfo, PioWrapper
from tools.deploy.verification import verify_device

# Fixed environment - hardcoded as requested
PIO_ENV = "esp32DedicatedStep"


@dataclass
class DeviceStatus:
    """Status of a single device deployment."""

    ip: str
    status: str = "pending"  # pending, uploading, verifying, success, failed
    progress: int = 0
    error: str | None = None
    firmware_version: str | None = None
    firmware_date: str | None = None


@dataclass
class DeploymentResult:
    """Overall deployment result for JSON export."""

    timestamp: str
    firmware_version: str
    firmware_date: str
    devices: list[dict]
    failed_ips: list[str] = field(default_factory=list)


class OtaDeployer:
    """Orchestrates parallel OTA deployment to multiple devices."""

    def __init__(
        self,
        config_path: Path,
        build: bool = True,
        deploy: bool = True,
        verify: bool = True,
        retry_file: Path | None = None,
        single_device: str | None = None,
        filesystem_only: bool = False,
        with_filesystem: bool = False,
        method: str | None = None,
    ):
        self.config_path = config_path
        self.build = build
        self.deploy = deploy
        self.verify = verify
        self.retry_file = retry_file
        self.single_device = single_device
        self.filesystem_only = filesystem_only
        self.with_filesystem = with_filesystem
        self.method_override = method  # CLI override; resolved in run()
        self.console = Console()
        self.project_dir = Path.cwd()
        self.pio: PioWrapper | None = None  # Created after loading config
        self.log_dir = self.project_dir / "tools" / "deploy" / "logs"
        self.log_dir.mkdir(parents=True, exist_ok=True)
        self.timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")

    def _load_config(self) -> tuple[list[str], str, str]:
        """Load device IPs, OTA password, and method from config file.

        Returns:
            Tuple of (list of IPs, OTA password, OTA method)
        """
        if not self.config_path.exists():
            self.console.print(f"[red]Config not found:[/] {self.config_path}")
            self.console.print("Copy ota_devices.toml.example and customize it.")
            sys.exit(1)

        try:
            with open(self.config_path, "rb") as f:
                config = tomllib.load(f)
                devices = config.get("devices", {})
                ips = devices.get("ips", [])
                if not ips:
                    self.console.print("[yellow]No devices configured in config file[/]")
                    sys.exit(1)
                ota_config = config.get("ota", {})
                password = ota_config.get("password", "")
                method = ota_config.get("method", "http")
                if not password:
                    self.console.print("[yellow]Warning: No OTA password configured[/]")
                return ips, password, method
        except tomllib.TOMLDecodeError as e:
            self.console.print(f"[red]Invalid TOML in config file:[/] {e}")
            sys.exit(1)

    def load_devices(self) -> list[str]:
        """Load device IPs from --device arg, retry file, or config file."""
        # Single device override (--device flag)
        if self.single_device:
            return [self.single_device]

        # Retry from previous failed deployment
        if self.retry_file:
            if not self.retry_file.exists():
                self.console.print(f"[red]Retry file not found:[/] {self.retry_file}")
                sys.exit(1)
            try:
                with open(self.retry_file) as f:
                    data = json.load(f)
                    failed = data.get("failed_ips", [])
                    if not failed:
                        self.console.print("[yellow]No failed devices in retry file[/]")
                        sys.exit(0)
                    return failed
            except json.JSONDecodeError as e:
                self.console.print(f"[red]Invalid JSON in retry file:[/] {e}")
                sys.exit(1)

        # Load from config file
        ips, _, _ = self._load_config()
        return ips

    async def run(self) -> int:
        """Run the deployment process.

        Returns:
            Exit code (0 for success, 1 for failures)
        """
        # Load config and initialize PioWrapper with password
        _, ota_password, config_method = self._load_config()
        self.method = self.method_override or config_method
        self.pio = PioWrapper(PIO_ENV, self.project_dir, ota_password=ota_password)

        # Step 1: Build phase
        firmware_info: FirmwareInfo | None = None
        fs_path: Path | None = None

        if self.build:
            # Build firmware (unless filesystem-only mode)
            if not self.filesystem_only:
                self.console.print("[bold]Building firmware...[/]")
                build_log = self.log_dir / f"{self.timestamp}_build.log"
                try:
                    firmware_info = await self.pio.build(build_log)
                    self.console.print(
                        f"[green]Build complete:[/] {firmware_info.version} ({firmware_info.date})"
                    )
                    self.console.print(f"[dim]Log: {build_log}[/]")
                except RuntimeError as e:
                    self.console.print(f"[red]Build failed:[/] {e}")
                    self.console.print(f"See log: {build_log}")
                    return 1

            # Build filesystem (if filesystem-only or with-filesystem mode)
            if self.filesystem_only or self.with_filesystem:
                self.console.print("[bold]Building filesystem...[/]")
                fs_log = self.log_dir / f"{self.timestamp}_buildfs.log"
                try:
                    fs_path = await self.pio.build_filesystem(fs_log)
                    self.console.print("[green]Filesystem build complete[/]")
                    self.console.print(f"[dim]Log: {fs_log}[/]")
                except RuntimeError as e:
                    self.console.print(f"[red]Filesystem build failed:[/] {e}")
                    self.console.print(f"See log: {fs_log}")
                    return 1
        else:
            # Skip-build mode: use existing artifacts
            if not self.filesystem_only:
                try:
                    firmware_info = await self.pio.get_firmware_info()
                    self.console.print(
                        f"[dim]Using existing build:[/] {firmware_info.version} ({firmware_info.date})"
                    )
                except FileNotFoundError as e:
                    self.console.print(f"[red]No existing build:[/] {e}")
                    self.console.print("Run without --skip-build to build first.")
                    return 1

            if self.filesystem_only or self.with_filesystem:
                try:
                    fs_path = self.pio.get_filesystem_path()
                    self.console.print(f"[dim]Using existing filesystem:[/] {fs_path.name}")
                except FileNotFoundError as e:
                    self.console.print(f"[red]No existing filesystem:[/] {e}")
                    return 1

        # Build-only mode: stop here
        if not self.deploy:
            self.console.print("\n[green]Build-only mode: skipping deployment[/]")
            return 0

        # Load devices for deployment
        devices = [DeviceStatus(ip=ip) for ip in self.load_devices()]
        if not devices:
            self.console.print("[yellow]No devices configured[/]")
            return 1

        # Step 2: Deploy firmware (if not filesystem-only)
        if not self.filesystem_only:
            assert firmware_info is not None
            self.console.print(f"\n[bold]Deploying firmware to {len(devices)} device(s)...[/]\n")
            await self._deploy_parallel(devices, firmware_info)

            # Print firmware summary
            self._print_summary(devices, firmware_info)
            self._save_summary(devices, firmware_info)

            # Check for failures before continuing to filesystem
            failed = [d for d in devices if d.status == "failed"]
            if failed and self.with_filesystem:
                self.console.print(
                    "\n[yellow]Skipping filesystem deploy due to firmware failures[/]"
                )
                return 1

        # Step 3: Deploy filesystem (if filesystem-only or with-filesystem)
        if self.filesystem_only or self.with_filesystem:
            assert fs_path is not None
            # Reset device status for filesystem deploy
            for d in devices:
                d.status = "pending"
                d.progress = 0
                d.error = None

            self.console.print(f"\n[bold]Deploying filesystem to {len(devices)} device(s)...[/]\n")
            await self._deploy_filesystem_parallel(devices, fs_path)

            # Print filesystem summary
            self._print_filesystem_summary(devices)

        failed = [d for d in devices if d.status == "failed"]
        return 1 if failed else 0

    async def _deploy_parallel(
        self, devices: list[DeviceStatus], firmware_info: FirmwareInfo
    ) -> None:
        """Deploy to all devices in parallel with live progress."""
        progress = Progress(
            SpinnerColumn(),
            TextColumn("[progress.description]{task.description}"),
            BarColumn(bar_width=30),
            TextColumn("{task.fields[status]}"),
            console=self.console,
        )

        # Create progress tasks for each device
        tasks: dict[str, TaskID] = {}
        for device in devices:
            task_id = progress.add_task(
                f"[cyan]{device.ip:15}[/]",
                total=100,
                status="Pending",
            )
            tasks[device.ip] = task_id

        async def deploy_single(device: DeviceStatus) -> None:
            """Deploy to a single device."""
            task_id = tasks[device.ip]
            log_file = self.log_dir / f"{self.timestamp}_{device.ip}.log"

            try:
                device.status = "uploading"
                progress.update(task_id, status="[yellow]Connecting...[/]")

                if self.method == "http":
                    upload_iter = self.pio.upload_http(device.ip, log_file, firmware_info.path)
                else:
                    upload_iter = self.pio.upload(device.ip, log_file, firmware_info.path)
                async for percent in upload_iter:
                    device.progress = percent
                    progress.update(
                        task_id, completed=percent, status=f"[yellow]Uploading {percent}%[/]"
                    )

                # Verify
                if self.verify:
                    device.status = "verifying"
                    progress.update(task_id, completed=100, status="[blue]Verifying...[/]")

                    result = await verify_device(
                        device.ip, firmware_info.version, firmware_info.date
                    )

                    if result.verified:
                        device.status = "success"
                        device.firmware_version = result.firmware_version
                        device.firmware_date = result.firmware_date
                        progress.update(task_id, status="[green]Success[/]")
                    else:
                        device.status = "failed"
                        device.error = result.error
                        device.firmware_version = result.firmware_version
                        device.firmware_date = result.firmware_date
                        progress.update(task_id, status="[red]Verify failed[/]")
                else:
                    device.status = "success"
                    progress.update(task_id, status="[green]Uploaded[/]")

            except RuntimeError as e:
                device.status = "failed"
                device.error = str(e)
                progress.update(task_id, status="[red]Upload failed[/]")

            except Exception as e:
                device.status = "failed"
                device.error = str(e)
                progress.update(task_id, status="[red]Error[/]")

        with Live(progress, console=self.console, refresh_per_second=4):
            await asyncio.gather(*[deploy_single(d) for d in devices])

    async def _deploy_filesystem_parallel(self, devices: list[DeviceStatus], fs_path: Path) -> None:
        """Deploy filesystem to all devices in parallel with live progress."""
        progress = Progress(
            SpinnerColumn(),
            TextColumn("[progress.description]{task.description}"),
            BarColumn(bar_width=30),
            TextColumn("{task.fields[status]}"),
            console=self.console,
        )

        tasks: dict[str, TaskID] = {}
        for device in devices:
            task_id = progress.add_task(
                f"[cyan]{device.ip:15}[/]",
                total=100,
                status="Pending",
            )
            tasks[device.ip] = task_id

        async def deploy_single(device: DeviceStatus) -> None:
            task_id = tasks[device.ip]
            log_file = self.log_dir / f"{self.timestamp}_{device.ip}_fs.log"

            try:
                device.status = "uploading"
                progress.update(task_id, status="[yellow]Connecting...[/]")

                assert self.pio is not None
                if self.method == "http":
                    fs_iter = self.pio.upload_filesystem_http(device.ip, log_file, fs_path)
                else:
                    fs_iter = self.pio.upload_filesystem(device.ip, log_file, fs_path)
                async for percent in fs_iter:
                    device.progress = percent
                    progress.update(
                        task_id, completed=percent, status=f"[yellow]Uploading {percent}%[/]"
                    )

                device.status = "success"
                progress.update(task_id, status="[green]Success[/]")

            except RuntimeError as e:
                device.status = "failed"
                device.error = str(e)
                progress.update(task_id, status="[red]Upload failed[/]")

            except Exception as e:
                device.status = "failed"
                device.error = str(e)
                progress.update(task_id, status="[red]Error[/]")

        with Live(progress, console=self.console, refresh_per_second=4):
            await asyncio.gather(*[deploy_single(d) for d in devices])

    def _print_filesystem_summary(self, devices: list[DeviceStatus]) -> None:
        """Print filesystem deployment summary table."""
        self.console.print()

        table = Table(title="Filesystem Deployment Summary")
        table.add_column("IP", style="cyan")
        table.add_column("Status", justify="center")
        table.add_column("Error", style="red")

        for device in devices:
            status_text = "[green]\u2714[/]" if device.status == "success" else "[red]\u2718[/]"
            table.add_row(
                device.ip,
                status_text,
                device.error or "",
            )

        self.console.print(table)

        succeeded = sum(1 for d in devices if d.status == "success")
        failed_devices = [d for d in devices if d.status == "failed"]

        self.console.print()
        if failed_devices:
            self.console.print(
                f"[green]Succeeded:[/] {succeeded}  [red]Failed:[/] {len(failed_devices)}"
            )
            self.console.print(f"[red]Failed IPs:[/] {', '.join(d.ip for d in failed_devices)}")
        else:
            self.console.print(f"[green]All {succeeded} device(s) filesystem deployed![/]")

        self.console.print(f"\n[dim]Logs saved to: {self.log_dir}[/]")

    def _print_summary(self, devices: list[DeviceStatus], firmware_info: FirmwareInfo) -> None:
        """Print deployment summary table."""
        self.console.print()

        table = Table(title="Deployment Summary")
        table.add_column("IP", style="cyan")
        table.add_column("Status", justify="center")
        table.add_column("Version")
        table.add_column("Date")
        table.add_column("Error", style="red")

        for device in devices:
            status_text = "[green]\u2714[/]" if device.status == "success" else "[red]\u2718[/]"
            table.add_row(
                device.ip,
                status_text,
                device.firmware_version or "-",
                device.firmware_date or "-",
                device.error or "",
            )

        self.console.print(table)

        # Summary stats
        succeeded = sum(1 for d in devices if d.status == "success")
        failed_devices = [d for d in devices if d.status == "failed"]

        self.console.print()
        if failed_devices:
            self.console.print(
                f"[green]Succeeded:[/] {succeeded}  [red]Failed:[/] {len(failed_devices)}"
            )
            self.console.print(f"[red]Failed IPs:[/] {', '.join(d.ip for d in failed_devices)}")
            summary_path = self.log_dir / f"{self.timestamp}_summary.json"
            self.console.print(
                f"\n[yellow]To retry failed devices:[/]\n"
                f"  poetry run python -m tools.deploy.ota_deploy --retry {summary_path}"
            )
        else:
            self.console.print(f"[green]All {succeeded} device(s) deployed successfully![/]")

        self.console.print(f"\n[dim]Logs saved to: {self.log_dir}[/]")

    def _save_summary(self, devices: list[DeviceStatus], firmware_info: FirmwareInfo) -> None:
        """Save deployment summary to JSON for retry support."""
        result = DeploymentResult(
            timestamp=self.timestamp,
            firmware_version=firmware_info.version,
            firmware_date=firmware_info.date,
            devices=[asdict(d) for d in devices],
            failed_ips=[d.ip for d in devices if d.status == "failed"],
        )

        summary_path = self.log_dir / f"{self.timestamp}_summary.json"
        with open(summary_path, "w") as f:
            json.dump(asdict(result), f, indent=2)


def main() -> int:
    """CLI entry point."""
    parser = argparse.ArgumentParser(
        description="Deploy firmware and/or filesystem to ESP32 devices via OTA",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  poetry run python -m tools.deploy.ota_deploy --device <IP>    # Firmware to single device
  poetry run python -m tools.deploy.ota_deploy                  # Firmware to all devices
  poetry run python -m tools.deploy.ota_deploy --filesystem     # Filesystem only
  poetry run python -m tools.deploy.ota_deploy --with-filesystem  # Both firmware and filesystem
  poetry run python -m tools.deploy.ota_deploy --build-only     # Build only, no deploy
  poetry run python -m tools.deploy.ota_deploy --skip-build     # Deploy existing build
  poetry run python -m tools.deploy.ota_deploy --retry <file>   # Retry failed devices
""",
    )
    parser.add_argument(
        "--build-only",
        action="store_true",
        help="Build firmware only, do not deploy",
    )
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="Skip build step, deploy existing firmware.bin",
    )
    parser.add_argument(
        "--no-verify",
        action="store_true",
        help="Skip POST-deploy API verification",
    )
    parser.add_argument(
        "--config",
        type=Path,
        default=Path("tools/deploy/ota_devices.toml"),
        help="Device config file (default: tools/deploy/ota_devices.toml)",
    )
    parser.add_argument(
        "--retry",
        type=Path,
        metavar="FILE",
        help="Retry failed devices from a previous summary.json",
    )
    parser.add_argument(
        "--device",
        type=str,
        metavar="IP",
        help="Deploy to a single device (overrides config file device list)",
    )
    parser.add_argument(
        "--filesystem",
        action="store_true",
        help="Deploy filesystem only (littlefs.bin), skip firmware",
    )
    parser.add_argument(
        "--with-filesystem",
        action="store_true",
        help="Deploy both firmware and filesystem",
    )
    parser.add_argument(
        "--method",
        choices=["http", "espota"],
        default=None,
        help="OTA upload method (default: from config, or 'http')",
    )

    args = parser.parse_args()

    # Validate mutually exclusive options
    if args.build_only and args.skip_build:
        parser.error("--build-only and --skip-build are mutually exclusive")
    if args.filesystem and args.with_filesystem:
        parser.error("--filesystem and --with-filesystem are mutually exclusive")

    deployer = OtaDeployer(
        config_path=args.config,
        build=not args.skip_build,
        deploy=not args.build_only,
        verify=not args.no_verify,
        retry_file=args.retry,
        single_device=args.device,
        filesystem_only=args.filesystem,
        with_filesystem=args.with_filesystem,
        method=args.method,
    )

    return asyncio.run(deployer.run())


if __name__ == "__main__":
    sys.exit(main())
