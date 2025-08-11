#!/usr/bin/env python3
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
import pytest

from tools.lower import lower_elf_to_arm, run_function

# For each sample, list of (inputs tuple -> expected return)
SAMPLES = {
    "arith.c": [
        ((1, 2), ((1 + 2) << 3) - 5),  # 19
        ((7, 3), ((7 + 3) << 3) - 5),  # 75
    ],
    "branches.c": [
        ((0,), 0),
        ((1,), 0),
        ((2,), -1),  # +0 -1
        ((3,), 1),   # +0 -1 +2
        ((10,), -5),  # (0-1)+(2-3)+...+(8-9) = -5
    ],
    "mem.c": [
        ((0,), 0),
        ((1,), 0),
        ((2,), 3),
        ((8,), 84),  # 3 * sum(0..7) = 84
    ],
    "fib.c": [
        ((0,), 0),
        ((1,), 1),
        ((2,), 1),
        ((5,), 5),
        ((10,), 55),
        ((20,), 6765),
    ],
}


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
    elf = out_dir / (sample_c.stem + ".elf")
    cmd = [
        clang,
        "--target=riscv64-unknown-elf",
        "-march=rv64i",
        "-mabi=lp64",
        "-ffreestanding",
        "-fno-builtin",
        "-fno-stack-protector",
        "-fno-asynchronous-unwind-tables",
        "-O0",
        str(sample_c),
        "-o",
        str(elf),
        "-nostdlib",
        "-Wl,-e,test",
        "-Wl,-static",
        "-fuse-ld=lld",
    ]
    subprocess.run(cmd, check=True)
    return elf


@pytest.mark.parametrize("sample", list(SAMPLES.keys()))
def test_lower_and_run_samples(sample: str, request):
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

        # Lower to ARM64 binary; place artifacts in the same temp dir
        res = lower_elf_to_arm(elf, build_dir=build_dir, out_dir=out_dir)
        # Exercise inputs/outputs for this sample
        for inputs, expected in SAMPLES[sample]:
            ret, _ = run_function(res.out_bin, inputs=list(inputs))
            print(f"{sample} inputs={inputs} -> ret={ret}")
            assert ret == expected


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--build-dir", default=str(Path(__file__).resolve().parents[2] / "build")
    )
    args, extra = parser.parse_known_args()
    os.environ["RISCY_BUILD_DIR"] = args.build_dir
    # -s disables capture, so prints are visible in CTest -V
    sys.exit(pytest.main([str(Path(__file__).resolve()), "-q", "-s"]))
