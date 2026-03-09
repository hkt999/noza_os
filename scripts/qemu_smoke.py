#!/usr/bin/env python3
import argparse
import os
import re
import signal
import socket
import subprocess
import sys
import time
from typing import Optional, Tuple


PROMPT_RE = re.compile(rb"noza(?:\((\d+)\))?> ")
PID_RE = re.compile(r"spawned pid (\d+)")


def reserve_port() -> int:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


def read_until_prompt(conn: socket.socket, timeout_s: float) -> Tuple[bytes, Optional[int]]:
    deadline = time.time() + timeout_s
    data = bytearray()
    last_prompt = None
    while time.time() < deadline:
        try:
            chunk = conn.recv(4096)
        except socket.timeout:
            continue
        if not chunk:
            break
        data.extend(chunk)
        matches = list(PROMPT_RE.finditer(data))
        if matches:
            group = matches[-1].group(1)
            last_prompt = int(group) if group is not None else None
            return bytes(data), last_prompt
    raise TimeoutError(f"timed out waiting for shell prompt; captured={data.decode('latin1', 'replace')}")


def connect_serial(port: int, timeout_s: float) -> socket.socket:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            conn = socket.create_connection(("127.0.0.1", port), timeout=0.5)
            conn.settimeout(0.2)
            return conn
        except OSError:
            time.sleep(0.1)
    raise TimeoutError(f"timed out connecting to QEMU serial on port {port}")


def run_smoke(kernel: str, qemu_bin: str, machine: str, boot_timeout_s: float) -> None:
    port = reserve_port()
    qemu = subprocess.Popen(
        [
            qemu_bin,
            "-M",
            machine,
            "-nographic",
            "-serial",
            f"tcp:127.0.0.1:{port},server",
            "-kernel",
            kernel,
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        preexec_fn=os.setsid,
    )
    conn = None
    try:
        conn = connect_serial(port, boot_timeout_s)
        _, current_prompt = read_until_prompt(conn, boot_timeout_s)

        checks = [
            (
                "help",
                [
                    "commands: ls [path], cat <file>, mkdir <path>, rm <path>, cd <path>, pwd, pid, ps, kill [-signum] <pid>, wait <pid>, exec <app>",
                    "unknown commands are resolved via /sbin and spawned with posix_spawnp",
                ],
            ),
            ("pwd", ["/"]),
            ("ls", ["sbin", "dev"]),
            ("ls /sbin", ["shell", "spin", "exit42"]),
            ("ps", ["PID", "/sbin/shell"]),
        ]

        for command, expected_fragments in checks:
            conn.sendall(command.encode("ascii") + b"\r\n")
            output, next_prompt = read_until_prompt(conn, boot_timeout_s)
            text = output.decode("latin1", "replace")
            for fragment in expected_fragments:
                if fragment not in text:
                    raise RuntimeError(
                        f"missing fragment {fragment!r} in output for command {command!r}: {text}"
                    )
            if current_prompt is not None and next_prompt is not None and next_prompt != current_prompt + 1:
                raise RuntimeError(
                    f"prompt counter skipped from {current_prompt} to {next_prompt} after {command!r}: {text}"
                )
            current_prompt = next_prompt

        conn.sendall(b"spin\r\n")
        output, next_prompt = read_until_prompt(conn, boot_timeout_s)
        text = output.decode("latin1", "replace")
        match = PID_RE.search(text)
        if not match:
            raise RuntimeError(f"failed to parse spawned spin pid: {text}")
        spin_pid = match.group(1)
        if current_prompt is not None and next_prompt is not None and next_prompt != current_prompt + 1:
            raise RuntimeError(f"prompt counter skipped from {current_prompt} to {next_prompt} after 'spin': {text}")
        current_prompt = next_prompt

        conn.sendall(b"exit42\r\n")
        output, next_prompt = read_until_prompt(conn, boot_timeout_s)
        text = output.decode("latin1", "replace")
        match = PID_RE.search(text)
        if not match:
            raise RuntimeError(f"failed to parse spawned exit42 pid: {text}")
        exit42_pid = match.group(1)
        if current_prompt is not None and next_prompt is not None and next_prompt != current_prompt + 1:
            raise RuntimeError(f"prompt counter skipped from {current_prompt} to {next_prompt} after 'exit42': {text}")
        current_prompt = next_prompt

        signal_checks = [
            ("ps", [exit42_pid, "/sbin/exit42", "EXIT"]),
            (f"wait {exit42_pid}", [f"wait: pid={exit42_pid} exit=42"]),
            ("ps", [f"{spin_pid}", "/sbin/spin", "RUN"]),
            (f"kill -19 {spin_pid}", []),
            ("ps", [f"{spin_pid}", "/sbin/spin", "STOP"]),
            (f"kill -18 {spin_pid}", []),
            ("ps", [f"{spin_pid}", "/sbin/spin", "RUN"]),
            (f"kill {spin_pid}", []),
            ("ps", [f"{spin_pid}", "/sbin/spin", "EXIT"]),
            (f"wait {spin_pid}", [f"wait: pid={spin_pid} signal=15"]),
        ]

        for command, expected_fragments in signal_checks:
            conn.sendall(command.encode("ascii") + b"\r\n")
            output, next_prompt = read_until_prompt(conn, boot_timeout_s)
            text = output.decode("latin1", "replace")
            for fragment in expected_fragments:
                if fragment not in text:
                    raise RuntimeError(
                        f"missing fragment {fragment!r} in output for command {command!r}: {text}"
                    )
            if current_prompt is not None and next_prompt is not None and next_prompt != current_prompt + 1:
                raise RuntimeError(
                    f"prompt counter skipped from {current_prompt} to {next_prompt} after {command!r}: {text}"
                )
            current_prompt = next_prompt

        conn.sendall(b"ps\r\n")
        output, next_prompt = read_until_prompt(conn, boot_timeout_s)
        text = output.decode("latin1", "replace")
        if exit42_pid in text or spin_pid in text:
            raise RuntimeError(f"reaped children still present in ps output: {text}")
        if current_prompt is not None and next_prompt is not None and next_prompt != current_prompt + 1:
            raise RuntimeError(
                f"prompt counter skipped from {current_prompt} to {next_prompt} after final 'ps': {text}"
            )
        current_prompt = next_prompt

        print("QEMU smoke test passed")
    finally:
        if conn is not None:
            conn.close()
        try:
            os.killpg(os.getpgid(qemu.pid), signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            qemu.wait(timeout=3)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(os.getpgid(qemu.pid), signal.SIGKILL)
            except ProcessLookupError:
                pass
            qemu.wait(timeout=3)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run a QEMU shell smoke test for Noza OS.")
    parser.add_argument("--kernel", required=True, help="Path to the QEMU kernel ELF")
    parser.add_argument("--qemu-bin", default="qemu-system-arm", help="QEMU binary")
    parser.add_argument("--machine", default="mps2-an385", help="QEMU machine name")
    parser.add_argument("--boot-timeout", type=float, default=8.0, help="Timeout in seconds")
    args = parser.parse_args()

    run_smoke(
        kernel=os.path.abspath(args.kernel),
        qemu_bin=args.qemu_bin,
        machine=args.machine,
        boot_timeout_s=args.boot_timeout,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
