import os
import platform
import shutil
import subprocess
from pathlib import Path


def print_tool(name):
    path = shutil.which(name)
    if path:
        print(f"{name}: {path}")
        return True
    print(f"{name}: not found")
    return False


def main():
    print("FluentPinyin environment check")
    print(f"OS: {platform.platform()}")
    print(f"64-bit process: {platform.machine().endswith('64')}")

    if print_tool("cmake"):
        subprocess.run(["cmake", "--version"], check=False)

    if not print_tool("cl.exe"):
        vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
        vswhere /= "Microsoft Visual Studio/Installer/vswhere.exe"
        if vswhere.exists():
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
                print(f"Visual Studio Build Tools: {path}")
                print("cl.exe is available through the Visual Studio generator.")
            else:
                print("MSVC x64 tools were not found by vswhere.")


if __name__ == "__main__":
    main()
