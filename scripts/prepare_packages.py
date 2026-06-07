import argparse
import hashlib
import json
import shutil
import ssl
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOWNLOADS_DIR = ROOT / "packages" / "downloads"
WINUI_DIR = ROOT / "packages" / "winui"
LIBRIME_DIR = ROOT / "third_party" / "librime"
WANXIANG_DIR = ROOT / "schemas" / "wanxiang" / "current"
FONT_DIR = ROOT / "assets" / "fonts"
ASSETS_PATH = DOWNLOADS_DIR / "m2-assets.json"
DEPENDENCIES_PATH = ROOT / "scripts" / "package_dependencies.json"
DOWNLOAD_TIMEOUT_SECONDS = 120
DOWNLOAD_RETRIES = 3


DEFAULT_ASSETS = {
    "webview2": "microsoft.web.webview2.1.0.3179.45.nupkg",
    "webview2Url": "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.3179.45",
    "cppwinrt": "microsoft.windows.cppwinrt.3.0.260520.1.nupkg",
    "cppwinrtUrl": "https://www.nuget.org/api/v2/package/Microsoft.Windows.CppWinRT/3.0.260520.1",
    "windowsAppSdkFoundation": "microsoft.windowsappsdk.foundation.1.8.260505001.nupkg",
    "windowsAppSdkFoundationUrl": "https://www.nuget.org/api/v2/package/Microsoft.WindowsAppSDK.Foundation/1.8.260505001",
    "windowsAppSdkWinui": "microsoft.windowsappsdk.winui.1.8.260505002.nupkg",
    "windowsAppSdkWinuiUrl": "https://www.nuget.org/api/v2/package/Microsoft.WindowsAppSDK.WinUI/1.8.260505002",
    "windowsAppSdkInteractive": "microsoft.windowsappsdk.interactiveexperiences.1.8.260430001.nupkg",
    "windowsAppSdkInteractiveUrl": "https://www.nuget.org/api/v2/package/Microsoft.WindowsAppSDK.InteractiveExperiences/1.8.260430001",
    "windowsAppRuntimeInstaller": "windowsappruntimeinstall-x64.exe",
    "windowsAppRuntimeInstallerUrl": "https://aka.ms/windowsappsdk/1.8/1.8.260508005/windowsappruntimeinstall-x64.exe",
    "librime": "rime-de4700e-Windows-msvc-x64.7z",
    "librimeUrl": "https://github.com/rime/librime/releases/download/1.16.1/rime-de4700e-Windows-msvc-x64.7z",
    "deps": "rime-deps-de4700e-Windows-msvc-x64.7z",
    "depsUrl": "https://github.com/rime/librime/releases/download/1.16.1/rime-deps-de4700e-Windows-msvc-x64.7z",
    "wanxiang": "rime-wanxiang-base.zip",
    "wanxiangUrl": "https://github.com/amzxyz/rime-wanxiang/releases/download/v15.11.1/rime-wanxiang-base.zip",
    "wanxiangGram": "wanxiang-lts-zh-hans.gram",
    "wanxiangGramUrl": "https://github.com/amzxyz/RIME-LMDG/releases/download/LTS/wanxiang-lts-zh-hans.gram",
    "miSans": "MiSans.zip",
    "miSansUrl": "https://hyperos.mi.com/font-download/MiSans.zip",
    "miSansTc": "MiSans_TC.zip",
    "miSansTcUrl": "https://hyperos.mi.com/font-download/MiSans_TC.zip",
    "miSansL3": "MiSans_L3.zip",
    "miSansL3Url": "https://hyperos.mi.com/font-download/MiSans_L3.zip",
    "sourceHanSansSc": "SourceHanSansSC.zip",
    "sourceHanSansScUrl": "https://github.com/adobe-fonts/source-han-sans/releases/download/2.005R/09_SourceHanSansSC.zip",
    "sourceHanSansTc": "SourceHanSansTC.zip",
    "sourceHanSansTcUrl": "https://github.com/adobe-fonts/source-han-sans/releases/download/2.005R/10_SourceHanSansTC.zip",
    "plangothicP1": "PlangothicP1-Regular.ttf",
    "plangothicP1Url": "https://github.com/Fitzgerald-Porthmouth-Koenigsegg/Plangothic_Project/releases/download/V2.9.5792/PlangothicP1-Regular.ttf",
    "plangothicP2": "PlangothicP2-Regular.ttf",
    "plangothicP2Url": "https://github.com/Fitzgerald-Porthmouth-Koenigsegg/Plangothic_Project/releases/download/V2.9.5792/PlangothicP2-Regular.ttf",
}


def load_dependencies():
    try:
        dependencies = json.loads(DEPENDENCIES_PATH.read_text(encoding="utf-8-sig"))
    except Exception as exc:
        raise RuntimeError(f"Cannot read dependency manifest {DEPENDENCIES_PATH}: {exc}") from exc
    result = {}
    for item in dependencies:
        try:
            dep_id = item["id"]
            name = item["name"]
            url = item["url"]
            sha256 = item["sha256"].lower()
            size = int(item["size"])
        except Exception as exc:
            raise RuntimeError(f"Invalid dependency manifest entry: {item}") from exc
        if len(sha256) != 64 or any(ch not in "0123456789abcdef" for ch in sha256):
            raise RuntimeError(f"Invalid SHA256 for dependency {dep_id}: {sha256}")
        result[dep_id] = {
            "id": dep_id,
            "name": name,
            "url": url,
            "sha256": sha256,
            "size": size,
        }
    return result


def ensure_dirs():
    for path in (DOWNLOADS_DIR, WINUI_DIR, LIBRIME_DIR, FONT_DIR):
        path.mkdir(parents=True, exist_ok=True)


def load_assets(dependencies):
    if not ASSETS_PATH.exists():
        ASSETS_PATH.write_text(json.dumps(DEFAULT_ASSETS, indent=2), encoding="utf-8")
        assets = dict(DEFAULT_ASSETS)
    else:
        try:
            assets = json.loads(ASSETS_PATH.read_text(encoding="utf-8-sig"))
        except Exception:
            assets = {}
        missing = [key for key in DEFAULT_ASSETS if key not in assets]
        if missing:
            assets = dict(DEFAULT_ASSETS)
            ASSETS_PATH.write_text(json.dumps(assets, indent=2), encoding="utf-8")

    updated = False
    for dep_id, dependency in dependencies.items():
        name_key = dep_id
        url_key = f"{dep_id}Url"
        if assets.get(name_key) != dependency["name"] or assets.get(url_key) != dependency["url"]:
            assets[name_key] = dependency["name"]
            assets[url_key] = dependency["url"]
            updated = True
    if updated:
        ASSETS_PATH.write_text(json.dumps(assets, indent=2), encoding="utf-8")
    return assets


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as input_file:
        for chunk in iter(lambda: input_file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download_file(url, destination):
    temp_path = destination.with_suffix(destination.suffix + ".tmp")
    temp_path.unlink(missing_ok=True)
    request = urllib.request.Request(
        url,
        headers={
            "User-Agent": "FluentPinyin-build/1.0",
            "Accept": "*/*",
        },
    )
    with urllib.request.urlopen(request, timeout=DOWNLOAD_TIMEOUT_SECONDS) as response:
        with temp_path.open("wb") as output:
            shutil.copyfileobj(response, output, length=1024 * 1024)
    temp_path.replace(destination)


def download_file_with_powershell(url, destination):
    temp_path = destination.with_suffix(destination.suffix + ".tmp")
    temp_path.unlink(missing_ok=True)
    command = [
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-Command",
        (
            "$ErrorActionPreference='Stop'; "
            "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; "
            "Invoke-WebRequest -Uri $args[0] -OutFile $args[1] "
            "-Headers @{ 'User-Agent' = 'FluentPinyin-build/1.0' }"
        ),
        url,
        str(temp_path),
    ]
    subprocess.run(command, check=True)
    temp_path.replace(destination)


def verify_asset_file(path, dependency):
    if not path.exists():
        raise RuntimeError(f"Missing dependency {dependency['id']}: {path}")
    actual_size = path.stat().st_size
    if actual_size != dependency["size"]:
        raise RuntimeError(
            f"Dependency size mismatch for {dependency['id']} ({path.name}): "
            f"expected {dependency['size']}, got {actual_size}"
        )
    actual_hash = sha256_file(path)
    if actual_hash != dependency["sha256"]:
        raise RuntimeError(
            f"Dependency SHA256 mismatch for {dependency['id']} ({path.name}): "
            f"expected {dependency['sha256']}, got {actual_hash}"
        )
    print(f"Verified {dependency['id']}: {path.name}")


def verify_all_downloads(dependencies):
    for dependency in dependencies.values():
        verify_asset_file(DOWNLOADS_DIR / dependency["name"], dependency)


def dependency_for_name(dependencies, name):
    for dependency in dependencies.values():
        if dependency["name"] == name:
            return dependency
    raise RuntimeError(f"No dependency manifest entry for {name}")


def get_asset_file(name, url, force, dependencies):
    dependency = dependency_for_name(dependencies, name)
    if dependency["url"] != url:
        raise RuntimeError(
            f"URL mismatch for {name}: expected {dependency['url']}, got {url}"
        )
    archive = DOWNLOADS_DIR / name
    last_error = None
    for attempt in range(1, DOWNLOAD_RETRIES + 1):
        if force or not archive.exists():
            print(f"Downloading {url}")
            try:
                download_file(url, archive)
            except (OSError, urllib.error.URLError) as exc:
                fallback_error = None
                if isinstance(exc, urllib.error.URLError) and isinstance(exc.reason, ssl.SSLError):
                    try:
                        print(f"Python TLS verification failed for {archive.name}; retrying with PowerShell.")
                        download_file_with_powershell(url, archive)
                    except (OSError, subprocess.CalledProcessError) as fallback_exc:
                        fallback_error = fallback_exc
                if fallback_error is not None or not archive.exists():
                    last_error = fallback_error or exc
                    archive.unlink(missing_ok=True)
                    if attempt >= DOWNLOAD_RETRIES:
                        raise RuntimeError(
                            f"Failed to download dependency {dependency['id']} after "
                            f"{DOWNLOAD_RETRIES} attempts: {last_error}"
                        ) from last_error
                    wait_seconds = attempt * 3
                    print(
                        f"Download failed for {archive.name}: {last_error}; "
                        f"retrying in {wait_seconds}s."
                    )
                    time.sleep(wait_seconds)
                    continue
        else:
            print(f"Using cached {archive}")
        try:
            verify_asset_file(archive, dependency)
            break
        except RuntimeError as exc:
            last_error = exc
            if force or attempt >= DOWNLOAD_RETRIES:
                raise
            print(f"Dependency {archive.name} failed verification: {exc}; downloading again.")
            archive.unlink(missing_ok=True)
            wait_seconds = attempt * 3
            time.sleep(wait_seconds)
    else:
        raise RuntimeError(f"Unable to prepare dependency {dependency['id']}: {last_error}")
    return archive


def expand_package(archive, destination, force):
    if force and destination.exists():
        shutil.rmtree(destination)
    if destination.exists():
        print(f"Using extracted {destination}")
        return
    destination.mkdir(parents=True, exist_ok=True)
    print(f"Extracting {archive} -> {destination}")
    subprocess.run(["tar", "-xf", str(archive), "-C", str(destination)], check=True)


def copy_file(source, destination, force):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if force or not destination.exists():
        shutil.copy2(source, destination)
        print(f"Copied {source} -> {destination}")
    else:
        print(f"Using copied {destination}")


def handle_fonts(assets, force, dependencies):
    font_downloads = [
        (
            assets["miSans"],
            assets["miSansUrl"],
            [
                ("MiSans/ttf/MiSans-Regular.ttf", "MiSans-Regular.ttf"),
                ("MiSans/ttf/MiSans-Medium.ttf", "MiSans-Medium.ttf"),
                ("MiSans/ttf/MiSans-Semibold.ttf", "MiSans-Semibold.ttf"),
            ],
        ),
        (
            assets["miSansTc"],
            assets["miSansTcUrl"],
            [
                ("MiSans TC/ttf/MisansTC-Regular.ttf", "MiSansTC-Regular.ttf"),
                ("MiSans TC/ttf/MisansTC-Medium.ttf", "MiSansTC-Medium.ttf"),
                ("MiSans TC/ttf/MisansTC-Semibold.ttf", "MiSansTC-Semibold.ttf"),
            ],
        ),
        (
            assets["miSansL3"],
            assets["miSansL3Url"],
            [("MiSans L3/MiSans L3.ttf", "MiSansL3-Regular.ttf")],
        ),
        (
            assets["sourceHanSansSc"],
            assets["sourceHanSansScUrl"],
            [("OTF/SimplifiedChinese/SourceHanSansSC-Regular.otf", "SourceHanSansSC-Regular.otf")],
        ),
        (
            assets["sourceHanSansTc"],
            assets["sourceHanSansTcUrl"],
            [("OTF/TraditionalChinese/SourceHanSansTC-Regular.otf", "SourceHanSansTC-Regular.otf")],
        ),
    ]

    for name, url, files in font_downloads:
        archive = get_asset_file(name, url, force, dependencies)
        extract_dir = DOWNLOADS_DIR / Path(name).stem
        needs_extract = force or not extract_dir.exists()
        if not needs_extract:
            needs_extract = any(not (FONT_DIR / dest).exists() for _, dest in files)
        if needs_extract:
            if extract_dir.exists():
                shutil.rmtree(extract_dir)
            extract_dir.mkdir(parents=True, exist_ok=True)
            print(f"Extracting {archive} -> {extract_dir}")
            subprocess.run(["tar", "-xf", str(archive), "-C", str(extract_dir)], check=True)
        else:
            print(f"Using extracted {extract_dir}")
        for source_rel, dest_name in files:
            source = extract_dir / source_rel
            if not source.exists():
                raise RuntimeError(f"Missing font inside archive: {source}")
            copy_file(source, FONT_DIR / dest_name, force)

    for key in ("plangothicP1", "plangothicP2"):
        source = get_asset_file(assets[key], assets[f"{key}Url"], force, dependencies)
        copy_file(source, FONT_DIR / assets[key], force)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()

    ensure_dirs()
    dependencies = load_dependencies()
    assets = load_assets(dependencies)
    if args.verify_only:
        verify_all_downloads(dependencies)
        print("Package download cache verified.")
        return

    nuget_packages = [
        (assets["webview2"], assets["webview2Url"], "microsoft.web.webview2.1.0.3179.45"),
        (assets["cppwinrt"], assets["cppwinrtUrl"], "microsoft.windows.cppwinrt.3.0.260520.1"),
        (
            assets["windowsAppSdkFoundation"],
            assets["windowsAppSdkFoundationUrl"],
            "microsoft.windowsappsdk.foundation.1.8.260505001",
        ),
        (
            assets["windowsAppSdkWinui"],
            assets["windowsAppSdkWinuiUrl"],
            "microsoft.windowsappsdk.winui.1.8.260505002",
        ),
        (
            assets["windowsAppSdkInteractive"],
            assets["windowsAppSdkInteractiveUrl"],
            "microsoft.windowsappsdk.interactiveexperiences.1.8.260430001",
        ),
    ]
    for name, url, directory in nuget_packages:
        archive = get_asset_file(name, url, args.force, dependencies)
        expand_package(archive, WINUI_DIR / directory, args.force)

    downloads = [
        (assets["librime"], assets["librimeUrl"], LIBRIME_DIR / "rime", True),
        (assets["deps"], assets["depsUrl"], LIBRIME_DIR / "deps", True),
        (assets["wanxiang"], assets["wanxiangUrl"], WANXIANG_DIR, True),
        (assets["wanxiangGram"], assets["wanxiangGramUrl"], WANXIANG_DIR / assets["wanxiangGram"], False),
    ]
    for name, url, destination, is_archive in downloads:
        archive = get_asset_file(name, url, args.force, dependencies)
        if is_archive:
            expand_package(archive, destination, args.force)
        else:
            copy_file(archive, destination, args.force)

    get_asset_file(
        assets["windowsAppRuntimeInstaller"],
        assets["windowsAppRuntimeInstallerUrl"],
        args.force,
        dependencies,
    )

    handle_fonts(assets, args.force, dependencies)
    print("Packages are ready.")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
