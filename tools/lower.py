#!/usr/bin/env python3
"""
Lower a RISC-V ELF to an AArch64 binary using the riscy pipeline.

This module exposes a reusable function `lower_elf_to_arm` for tests and a
command-line interface for ad-hoc use.
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


@dataclass
class LowerResult:
    elf: Path
    build_dir: Path
    asm_path: Path
    obj_path: Path
    out_bin: Path
    # Keep reference to temp dir (if used) so it isn't GC'd immediately
    _tmpdir: Optional[object] = None


def _discover_build_dir(explicit: Optional[Path]) -> Path:
    if explicit:
        return explicit
    env = os.environ.get("RISCY_BUILD_DIR")
    if env:
        return Path(env)
    return Path(__file__).resolve().parents[1] / "build"


def _require_file(p: Path, what: str) -> None:
    if not p.exists():
        raise FileNotFoundError(f"{what} not found at {p}")


def _run(cmd: list[str], cwd: Optional[Path] = None) -> None:
    proc = subprocess.run(
        cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )
    if proc.returncode != 0:
        raise RuntimeError(
            f"command failed: {' '.join(cmd)}\nstdout:\n{proc.stdout}\nstderr:\n{proc.stderr}"
        )


def lower_elf_to_arm(
    elf_path: Path | str,
    *,
    build_dir: Optional[Path] = None,
    out_dir: Optional[Path] = None,
    out_path: Optional[Path] = None,
    keep_intermediates: bool = False,
) -> LowerResult:
    """
    Translate a RISC-V ELF into an AArch64 binary.

    - Uses `riscy` to emit AArch64 assembly
    - Assembles with host `clang` for arm64
    - Links with `libriscy_runtime.a`

    Returns a LowerResult with paths to intermediates and the final binary.
    Raises on failures (missing tools/files or non-zero command exits).
    """
    elf_path = Path(elf_path).resolve()
    _require_file(elf_path, "input ELF")

    bdir = _discover_build_dir(build_dir).resolve()
    riscy_bin = bdir / "riscy"
    runtime_lib = bdir / "libriscy_runtime.a"
    _require_file(riscy_bin, "riscy binary")
    _require_file(runtime_lib, "runtime library")

    clang = shutil.which("clang")
    if not clang:
        raise RuntimeError("clang not found in PATH")

    # Working directory for intermediates
    tmp_ctx: Optional[tempfile.TemporaryDirectory] = None
    if out_dir is None and not keep_intermediates:
        tmp_ctx = tempfile.TemporaryDirectory()
        work_dir = Path(tmp_ctx.name)
    else:
        work_dir = (out_dir or Path.cwd()).resolve()
        work_dir.mkdir(parents=True, exist_ok=True)

    asm_path = work_dir / (elf_path.stem + ".s")
    obj_path = work_dir / (elf_path.stem + ".o")
    out_bin = Path(out_path) if out_path else work_dir / (elf_path.stem + ".arm64")

    # 1) Emit AArch64 assembly
    _run([str(riscy_bin), "--aarch64", str(asm_path), str(elf_path)])

    # 2) Assemble to object
    _run([clang, "-arch", "arm64", "-c", str(asm_path), "-o", str(obj_path)])

    # 3) Link with runtime
    _run([clang, "-arch", "arm64", str(obj_path), str(runtime_lib), "-o", str(out_bin)])

    return LowerResult(
        elf=elf_path,
        build_dir=bdir,
        asm_path=asm_path,
        obj_path=obj_path,
        out_bin=out_bin,
        _tmpdir=tmp_ctx,
    )


def _normalize_inputs_dict(inputs: dict[str, int]) -> list[int]:
    # Map provided a0..a7/x10..x17 into ordered positional list a0..a7.
    vals = [0] * 8
    seen = [False] * 8
    for k, v in inputs.items():
        k = k.strip()
        idx: Optional[int] = None
        if k.startswith("a") and k[1:].isdigit():
            i = int(k[1:])
            if 0 <= i <= 7:
                idx = i
        elif k.startswith("x") and k[1:].isdigit():
            i = int(k[1:])
            if 10 <= i <= 17:
                idx = i - 10
        if idx is None:
            raise ValueError(f"unsupported input register key: {k}")
        vals[idx] = int(v)
        seen[idx] = True
    # Trim trailing zeros that were not provided to minimize argv
    last = -1
    for i in range(7, -1, -1):
        if seen[i]:
            last = i
            break
    if last >= 0:
        return vals[: last + 1]
    return []


def run_function(bin_path: Path | str, inputs: list[int] | None = None, dump: list[str] | None = None) -> tuple[int, dict[str, int]]:
    """
    Run the produced AArch64 binary treating the entry as a function.

    - Positional inputs map to a0..a7.
    - Captures the function return (a0) from the "RET <val>" line.
    - Optionally parses any OUT xN=... lines if dump is provided.

    Returns (ret_value, outs_dict).
    """
    bin_path = Path(bin_path)
    if not bin_path.exists():
        raise FileNotFoundError(f"binary not found: {bin_path}")
    args = [str(bin_path)]
    if inputs:
        args += [str(int(v)) for v in inputs]
    if dump:
        args.append("--dump=" + ",".join(dump))
    proc = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if proc.returncode != 0:
        raise RuntimeError(
            f"binary failed: {' '.join(args)}\nstdout:\n{proc.stdout}\nstderr:\n{proc.stderr}"
        )
    ret: Optional[int] = None
    outs: dict[str, int] = {}
    for ln in proc.stdout.splitlines():
        ln = ln.strip()
        if ln.startswith("RET "):
            try:
                ret = int(ln.split(" ", 1)[1], 10)
            except Exception:
                pass
        elif ln.startswith("OUT ") and "=" in ln:
            body = ln[4:]
            name, val = body.split("=", 1)
            name = name.strip()
            try:
                outs[name] = int(val, 10)
            except ValueError:
                pass
    if ret is None:
        raise RuntimeError(f"did not find RET line in output:\n{proc.stdout}")
    return ret, outs


def run_binary_with_inputs(
    bin_path: Path | str,
    inputs: dict[str, int] | None = None,
    dump: list[str] | None = None,
) -> dict[str, int]:
    """
    Compatibility helper: run with dict inputs (a0..a7 or x10..x17) and optionally dump extra regs.
    Returns a mapping of any requested OUT registers (if dump provided).
    """
    pos = _normalize_inputs_dict(inputs or {})
    ret, outs = run_function(bin_path, inputs=pos, dump=dump)
    # Include return as x10 in outs for convenience
    outs.setdefault("x10", ret)
    return outs


def _main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description="Lower RISC-V ELF to AArch64 binary")
    ap.add_argument("elf", type=Path, help="Path to RISC-V input ELF")
    ap.add_argument("-o", "--output", type=Path, help="Output binary path")
    ap.add_argument(
        "--build-dir",
        type=Path,
        help="Build directory containing riscy and libriscy_runtime.a (defaults to $RISCY_BUILD_DIR or ./build)",
    )
    ap.add_argument(
        "--out-dir", type=Path, help="Directory for intermediates and default output"
    )
    ap.add_argument(
        "--keep-intermediates",
        action="store_true",
        help="Keep generated assembly/object files",
    )
    args = ap.parse_args(argv)

    try:
        res = lower_elf_to_arm(
            args.elf,
            build_dir=args.build_dir,
            out_dir=args.out_dir,
            out_path=args.output,
            keep_intermediates=args.keep_intermediates,
        )
    except Exception as e:
        print(f"error: {e}", file=sys.stderr)
        return 1

    print(f"wrote AArch64 binary: {res.out_bin}")
    return 0


if __name__ == "__main__":
    sys.exit(_main(sys.argv[1:]))
