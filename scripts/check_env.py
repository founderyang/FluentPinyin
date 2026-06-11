import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BUILD_DIRS = ("build-release", "build-debug")


def norm_path(path):
    return os.path.normcase(str(Path(path).resolve(strict=False)))


def print_status(label, ok, detail=""):
    status = "ok" if ok else "missing"
    suffix = f": {detail}" if detail else ""
    print(f"{label}: {status}{suffix}")


def print_tool(name):
    path = shutil.which(name)
    if path:
        print(f"{name}: {path}")
        return path
    print(f"{name}: not found")
    return ""


def find_wix_tool():
    path = shutil.which("wix.exe")
    if path:
        return path
    program_files = Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
    fallback = program_files / "WiX Toolset v7.0" / "bin" / "wix.exe"
    if fallback.exists():
        return str(fallback)
    return ""


def find_visual_studio_build_tools():
    if shutil.which("cl.exe"):
        return ("cl.exe is on PATH", True)

    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
    vswhere /= "Microsoft Visual Studio/Installer/vswhere.exe"
    if not vswhere.exists():
        return (f"vswhere not found at {vswhere}", False)

    result = subprocess.run(
        [
            str(vswhere),
            "-latest",
            "-products",
            "*",
            "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property",
            "installationPath",
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    path = result.stdout.strip()
    if path:
        return (path, True)
    return ("MSVC x64 tools were not found by vswhere", False)


def read_cmake_cache_value(cache_path, key):
    if not cache_path.exists():
        return ""
    prefix = f"{key}:"
    for line in cache_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(prefix) and "=" in line:
            return line.split("=", 1)[1].strip()
    return ""


def check_build_dir_source(build_dir):
    cache_path = build_dir / "CMakeCache.txt"
    if not cache_path.exists():
        print(f"{build_dir.name}: no CMake cache")
        return True

    configured_source = read_cmake_cache_value(cache_path, "CMAKE_HOME_DIRECTORY")
    if not configured_source:
        print(f"{build_dir.name}: CMake cache has no source directory")
        return False

    if norm_path(configured_source) != norm_path(ROOT):
        print(
            f"{build_dir.name}: configured for {configured_source}; "
            f"current checkout is {ROOT}"
        )
        return False

    print(f"{build_dir.name}: source directory matches current checkout")
    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--build-dir",
        action="append",
        default=[],
        help="Build directory to verify against this checkout. May be passed more than once.",
    )
    parser.add_argument(
        "--skip-build-dir-check",
        action="store_true",
        help="Do not verify existing CMake build directories.",
    )
    args = parser.parse_args()

    ok = True
    print("FluentPinyin environment check")
    print(f"OS: {platform.platform()}")
    is_windows = platform.system() == "Windows"
    is_64_bit = platform.machine().endswith("64")
    print_status("Windows", is_windows)
    print_status("64-bit process", is_64_bit, platform.machine())
    ok = ok and is_windows and is_64_bit

    cmake = print_tool("cmake")
    if cmake:
        subprocess.run(["cmake", "--version"], check=False)
    else:
        ok = False

    msvc_detail, has_msvc = find_visual_studio_build_tools()
    print_status("MSVC x64 tools", has_msvc, msvc_detail)
    ok = ok and has_msvc

    print_tool("git")
    wix = find_wix_tool()
    if wix:
        print(f"wix.exe: {wix}")
    else:
        print("wix.exe: not found")
    print_tool("gh")

    if not args.skip_build_dir_check:
        build_dirs = args.build_dir or list(DEFAULT_BUILD_DIRS)
        for build_dir in build_dirs:
            ok = check_build_dir_source((ROOT / build_dir).resolve(strict=False)) and ok

    if not ok:
        print("Environment check failed.")
        return 1
    print("Environment check passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
