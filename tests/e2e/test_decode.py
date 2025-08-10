#!/usr/bin/env python3
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
import pytest
import sys

SAMPLES = [
    "arith.c",
    "branches.c",
    "mem.c",
]


def have_toolchain() -> tuple[bool, str]:
    clang = shutil.which("clang")
    if not clang:
        return False, "clang not found in PATH"
    # Probe compile to object; avoids needing lld at this stage
    with tempfile.TemporaryDirectory() as td:
        src = Path(td) / "probe.c"
        src.write_text("void _start(void){}\n")
        obj = Path(td) / "probe.o"
        cmd = [
            clang,
            "--target=riscv64-unknown-elf",
            "-ffreestanding",
            "-fno-builtin",
            "-fno-stack-protector",
            "-O2",
            "-c",
            str(src),
            "-o",
            str(obj),
        ]
        try:
            res = subprocess.run(
                cmd,
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            return obj.exists(), ""
        except subprocess.CalledProcessError as e:
            return False, f"probe compile failed: {e.stderr.strip()}"
        except Exception as e:
            return False, f"toolchain probe error: {e}"


def build_sample(clang: str, sample_c: Path, out_dir: Path) -> Path:
    out_dir.mkdir(parents=True, exist_ok=True)
    obj = out_dir / (sample_c.stem + ".o")
    elf = out_dir / (sample_c.stem + ".elf")
    cflags = [
        "--target=riscv64-unknown-elf",
        "-march=rv64i",
        "-mabi=lp64",
        "-ffreestanding",
        "-fno-builtin",
        "-fno-stack-protector",
        "-fno-asynchronous-unwind-tables",
        "-O0",
        "-c",
    ]
    ldflags = [
        "--target=riscv64-unknown-elf",
        "-march=rv64i",
        "-mabi=lp64",
        "-nostdlib",
        "-Wl,-e,_start",
        "-Wl,-static",
        "-fuse-ld=lld",
    ]
    subprocess.run([clang] + cflags + [str(sample_c), "-o", str(obj)], check=True)
    subprocess.run([clang] + ldflags + [str(obj), "-o", str(elf)], check=True)
    return elf


def run_riscy(riscy_bin: Path, elf: Path) -> subprocess.CompletedProcess:
    args = [str(riscy_bin), "--cfg", str(elf)]
    return subprocess.run(
        args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )


@pytest.mark.parametrize("sample", SAMPLES)
def test_decode_samples(sample: str, request):
    ok, reason = have_toolchain()
    if not ok:
        print(f"SKIP: {reason}")
        pytest.skip(f"RISC-V toolchain unavailable: {reason}")

    # Prefer env provided by CTest; fallback to ../build
    build_dir = Path(
        os.environ.get(
            "RISCY_BUILD_DIR", str(Path(__file__).resolve().parents[2] / "build")
        )
    )
    riscy_bin = build_dir / "riscy"
    if not riscy_bin.exists():
        msg = f"riscy binary not found at {riscy_bin}"
        print(f"SKIP: {msg}")
        pytest.skip(msg)

    clang = shutil.which("clang")
    assert clang, "clang must be available"

    repo_root = Path(__file__).resolve().parents[2]
    sample_c = repo_root / "tests" / "e2e" / "samples" / sample

    with tempfile.TemporaryDirectory() as td:
        out_dir = Path(td)
        try:
            elf = build_sample(clang, sample_c, out_dir)
        except subprocess.CalledProcessError as e:
            pytest.skip(f"Failed to build sample with clang: {e}")

        proc = run_riscy(riscy_bin, elf)
        # Always print decoded output (pytest -s in __main__ ensures visibility under CTest -V)
        print(f"=== riscy decode: {sample} ===")
        out_lines = [ln for ln in proc.stdout.splitlines() if ln.strip()]
        for ln in out_lines:
            print(ln)
        assert proc.returncode == 0, f"riscy failed: {proc.stderr}"
        assert "<decode error>" not in proc.stdout, proc.stdout
        # Ensure at least a few lines decoded
        assert len(out_lines) >= 1, "no decode output produced"


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--build-dir", default=str(Path(__file__).resolve().parents[2] / "build")
    )
    args, extra = parser.parse_known_args()
    os.environ["RISCY_BUILD_DIR"] = args.build_dir
    # -s disables capture, so prints are visible in CTest -V
    sys.exit(pytest.main([str(Path(__file__).resolve()), "-q", "-s"]))
