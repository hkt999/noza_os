#!/usr/bin/env python3
import argparse
import os
import re
import signal
import socket
import subprocess
import sys
import time
from typing import Optional, TextIO, Tuple


PROMPT_RE = re.compile(rb"noza(?:\((\d+)\))?> ")
PID_RE = re.compile(r"spawned pid (\d+)")
UNITY_RE = re.compile(r"(\d+)\s+Tests\s+(\d+)\s+Failures\s+(\d+)\s+Ignored")


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


class QemuStressSession:
    def __init__(self, kernel: str, qemu_bin: str, machine: str, boot_timeout_s: float, log_fp: Optional[TextIO]):
        self.kernel = kernel
        self.qemu_bin = qemu_bin
        self.machine = machine
        self.boot_timeout_s = boot_timeout_s
        self.log_fp = log_fp
        self.current_prompt: Optional[int] = None
        self.qemu: Optional[subprocess.Popen] = None
        self.conn: Optional[socket.socket] = None

    def log(self, message: str) -> None:
        print(message)
        if self.log_fp is not None:
            self.log_fp.write(message + "\n")
            self.log_fp.flush()

    def log_output(self, header: str, text: str) -> None:
        if self.log_fp is None:
            return
        self.log_fp.write(f"{header}\n")
        self.log_fp.write(text)
        if not text.endswith("\n"):
            self.log_fp.write("\n")
        self.log_fp.flush()

    def __enter__(self) -> "QemuStressSession":
        port = reserve_port()
        self.qemu = subprocess.Popen(
            [
                self.qemu_bin,
                "-M",
                self.machine,
                "-nographic",
                "-serial",
                f"tcp:127.0.0.1:{port},server",
                "-kernel",
                self.kernel,
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            preexec_fn=os.setsid,
        )
        self.conn = connect_serial(port, self.boot_timeout_s)
        output, self.current_prompt = read_until_prompt(self.conn, self.boot_timeout_s)
        self.log_output("=== boot ===", output.decode("latin1", "replace"))
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        if self.conn is not None:
            self.conn.close()
        if self.qemu is not None:
            try:
                os.killpg(os.getpgid(self.qemu.pid), signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                self.qemu.wait(timeout=3)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(os.getpgid(self.qemu.pid), signal.SIGKILL)
                except ProcessLookupError:
                    pass
                self.qemu.wait(timeout=3)

    def run_command(
        self,
        command: str,
        timeout_s: float,
        expected_fragments: Optional[Tuple[str, ...]] = None,
    ) -> str:
        assert self.conn is not None
        self.conn.sendall(command.encode("ascii") + b"\r\n")
        output, next_prompt = read_until_prompt(self.conn, timeout_s)
        text = output.decode("latin1", "replace")
        self.log_output(f"=== {command} ===", text)

        if expected_fragments:
            for fragment in expected_fragments:
                if fragment not in text:
                    raise RuntimeError(
                        f"missing fragment {fragment!r} in output for command {command!r}: {text}"
                    )
        if self.current_prompt is not None and next_prompt is not None and next_prompt != self.current_prompt + 1:
            raise RuntimeError(
                f"prompt counter skipped from {self.current_prompt} to {next_prompt} after {command!r}: {text}"
            )
        self.current_prompt = next_prompt
        return text

    def run_unity_command(self, command: str, timeout_s: float, min_tests: int = 1) -> None:
        text = self.run_command(command, timeout_s)
        match = UNITY_RE.search(text)
        if not match:
            raise RuntimeError(f"missing Unity summary for command {command!r}: {text}")
        tests, failures, ignored = (int(match.group(i)) for i in range(1, 4))
        if tests < min_tests:
            raise RuntimeError(f"unexpected Unity test count for {command!r}: {tests}")
        if failures != 0 or ignored != 0:
            raise RuntimeError(
                f"Unity suite reported failures for {command!r}: tests={tests} failures={failures} ignored={ignored}"
            )

    def spawn_and_wait_unity(self, command: str, suite_timeout_s: float, min_tests: int = 1) -> None:
        spawn_text = self.run_command(command, suite_timeout_s)
        match = PID_RE.search(spawn_text)
        if not match:
            raise RuntimeError(f"failed to parse spawned pid for command {command!r}: {spawn_text}")
        pid = match.group(1)
        wait_text = self.run_command(f"wait {pid}", suite_timeout_s, (f"wait: pid={pid} exit=0",))
        summary = UNITY_RE.search(wait_text)
        if not summary:
            raise RuntimeError(f"missing Unity summary while waiting for command {command!r}: {wait_text}")
        tests, failures, ignored = (int(summary.group(i)) for i in range(1, 4))
        if tests < min_tests:
            raise RuntimeError(f"unexpected Unity test count for {command!r}: {tests}")
        if failures != 0 or ignored != 0:
            raise RuntimeError(
                f"Unity suite reported failures for {command!r}: tests={tests} failures={failures} ignored={ignored}"
            )

    def sanity_check(self, command_timeout_s: float) -> None:
        self.run_command("help", command_timeout_s, ("commands:", "spawned with posix_spawnp"))
        self.run_command("ls /sbin", command_timeout_s, ("shell", "spin", "exit42", "noza_unittest", "posix_unittest", "futex_test"))

    def process_stress(self, iterations: int, command_timeout_s: float) -> None:
        for index in range(1, iterations + 1):
            self.log(f"[spawn] iteration {index}/{iterations}")

            spin_text = self.run_command("spin", command_timeout_s)
            spin_match = PID_RE.search(spin_text)
            if not spin_match:
                raise RuntimeError(f"failed to parse spawned spin pid: {spin_text}")
            spin_pid = spin_match.group(1)

            exit_text = self.run_command("exit42", command_timeout_s)
            exit_match = PID_RE.search(exit_text)
            if not exit_match:
                raise RuntimeError(f"failed to parse spawned exit42 pid: {exit_text}")
            exit_pid = exit_match.group(1)

            self.run_command("ps", command_timeout_s, (spin_pid, "/sbin/spin", "RUN", exit_pid, "/sbin/exit42", "EXIT"))
            self.run_command(f"wait {exit_pid}", command_timeout_s, (f"wait: pid={exit_pid} exit=42",))
            self.run_command(f"kill -19 {spin_pid}", command_timeout_s)
            self.run_command("ps", command_timeout_s, (spin_pid, "/sbin/spin", "STOP"))
            self.run_command(f"kill -18 {spin_pid}", command_timeout_s)
            self.run_command("ps", command_timeout_s, (spin_pid, "/sbin/spin", "RUN"))
            self.run_command(f"kill {spin_pid}", command_timeout_s)
            self.run_command("ps", command_timeout_s, (spin_pid, "/sbin/spin", "EXIT"))
            self.run_command(f"wait {spin_pid}", command_timeout_s, (f"wait: pid={spin_pid} signal=15",))
            ps_text = self.run_command("ps", command_timeout_s)
            if spin_pid in ps_text or exit_pid in ps_text:
                raise RuntimeError(f"reaped children still visible after iteration {index}: {ps_text}")

    def suite_stress(self, name: str, command: str, iterations: int, timeout_s: float, min_tests: int) -> None:
        for index in range(1, iterations + 1):
            self.log(f"[{name}] iteration {index}/{iterations}")
            self.spawn_and_wait_unity(command, timeout_s, min_tests=min_tests)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run repeated QEMU stress checks for Noza OS.")
    parser.add_argument("--kernel", required=True, help="Path to the QEMU kernel ELF")
    parser.add_argument("--qemu-bin", default="qemu-system-arm", help="QEMU binary")
    parser.add_argument("--machine", default="mps2-an385", help="QEMU machine name")
    parser.add_argument("--boot-timeout", type=float, default=8.0, help="Boot timeout in seconds")
    parser.add_argument("--command-timeout", type=float, default=8.0, help="Timeout for shell/process commands in seconds")
    parser.add_argument("--suite-timeout", type=float, default=60.0, help="Timeout for unit-test commands in seconds")
    parser.add_argument("--spawn-iterations", type=int, default=10, help="Iterations for spawn/kill/wait stress")
    parser.add_argument("--futex-iterations", type=int, default=5, help="Iterations for futex_test")
    parser.add_argument("--posix-heavy-iterations", type=int, default=5, help="Iterations for posix heavy-loading test")
    parser.add_argument("--posix-spin-iterations", type=int, default=5, help="Iterations for posix spinlock test")
    parser.add_argument("--noza-iterations", type=int, default=3, help="Iterations for the full noza_unittest suite")
    parser.add_argument("--log", help="Optional path for a full transcript log")
    args = parser.parse_args()

    log_fp = open(args.log, "w", encoding="utf-8") if args.log else None
    try:
        with QemuStressSession(
            kernel=os.path.abspath(args.kernel),
            qemu_bin=args.qemu_bin,
            machine=args.machine,
            boot_timeout_s=args.boot_timeout,
            log_fp=log_fp,
        ) as session:
            session.sanity_check(args.command_timeout)
            session.process_stress(args.spawn_iterations, args.command_timeout)
            session.suite_stress("futex", "futex_test", args.futex_iterations, args.suite_timeout, min_tests=2)
            session.suite_stress(
                "posix-heavy",
                "posix_unittest test_heavy_loading",
                args.posix_heavy_iterations,
                args.suite_timeout,
                min_tests=1,
            )
            session.suite_stress(
                "posix-spin",
                "posix_unittest test_pthread_spinlock",
                args.posix_spin_iterations,
                args.suite_timeout,
                min_tests=1,
            )
            session.suite_stress("noza", "noza_unittest", args.noza_iterations, args.suite_timeout, min_tests=23)
            session.log("QEMU stress test passed")
    finally:
        if log_fp is not None:
            log_fp.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
