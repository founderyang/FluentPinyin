import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.request
import uuid
import zipfile
from pathlib import Path
from xml.sax.saxutils import escape


ROOT = Path(__file__).resolve().parents[1]
LICENSE_CACHE_DIR = ROOT / "packages" / "licenses"

REMOTE_LICENSES = (
    ("librime-LICENSE.txt", "https://raw.githubusercontent.com/rime/librime/master/LICENSE"),
    ("librime-lua-LICENSE.txt", "https://raw.githubusercontent.com/hchunhui/librime-lua/master/LICENSE"),
    ("librime-octagram-LICENSE.txt", "https://raw.githubusercontent.com/lotem/librime-octagram/master/LICENSE"),
    ("librime-predict-LICENSE.txt", "https://raw.githubusercontent.com/rime/librime-predict/master/LICENSE"),
    ("OpenCC-LICENSE.txt", "https://raw.githubusercontent.com/BYVoid/OpenCC/master/LICENSE"),
    ("marisa-trie-COPYING.md", "https://raw.githubusercontent.com/s-yata/marisa-trie/master/COPYING.md"),
    ("yaml-cpp-LICENSE.txt", "https://raw.githubusercontent.com/jbeder/yaml-cpp/master/LICENSE"),
    ("LevelDB-LICENSE.txt", "https://raw.githubusercontent.com/google/leveldb/main/LICENSE"),
    ("glog-LICENSE.md", "https://raw.githubusercontent.com/google/glog/master/LICENSE.md"),
    (
        "Fluent-UI-System-Icons-LICENSE.txt",
        "https://raw.githubusercontent.com/microsoft/fluentui-system-icons/main/LICENSE",
    ),
    (
        "Plangothic-LICENSE-MIT.txt",
        "https://raw.githubusercontent.com/Fitzgerald-Porthmouth-Koenigsegg/Plangothic_Project/main/LICENSE-MIT.txt",
    ),
    (
        "Plangothic-LICENSE-OFL.txt",
        "https://raw.githubusercontent.com/Fitzgerald-Porthmouth-Koenigsegg/Plangothic_Project/main/LICENSE-OFL.txt",
    ),
)

LOCAL_LICENSES = (
    (
        ROOT / "packages" / "winui" / "microsoft.web.webview2.1.0.3179.45" / "LICENSE.txt",
        "Microsoft-WebView2-SDK-LICENSE.txt",
    ),
    (
        ROOT / "packages" / "winui" / "microsoft.web.webview2.1.0.3179.45" / "NOTICE.txt",
        "Microsoft-WebView2-SDK-NOTICE.txt",
    ),
    (
        ROOT / "packages" / "winui" / "microsoft.windows.cppwinrt.3.0.260520.1" / "LICENSE",
        "Microsoft-CppWinRT-LICENSE.txt",
    ),
    (
        ROOT / "packages" / "winui" / "microsoft.windowsappsdk.foundation.1.8.260505001" / "license.txt",
        "WindowsAppSDK-Foundation-LICENSE.txt",
    ),
    (
        ROOT / "packages" / "winui" / "microsoft.windowsappsdk.winui.1.8.260505002" / "license.txt",
        "WindowsAppSDK-WinUI-LICENSE.txt",
    ),
    (
        ROOT / "packages" / "winui" / "microsoft.windowsappsdk.winui.1.8.260505002" / "NOTICE.txt",
        "WindowsAppSDK-WinUI-NOTICE.txt",
    ),
    (
        ROOT / "packages" / "winui" / "microsoft.windowsappsdk.winui.1.8.260505002" / "tools" / "NOTICE.txt",
        "WindowsAppSDK-WinUI-Tools-NOTICE.txt",
    ),
    (
        ROOT
        / "packages"
        / "winui"
        / "microsoft.windowsappsdk.interactiveexperiences.1.8.260430001"
        / "license.txt",
        "WindowsAppSDK-InteractiveExperiences-LICENSE.txt",
    ),
    (
        ROOT / "packages" / "downloads" / "SourceHanSansSC" / "LICENSE.txt",
        "SourceHanSansSC-LICENSE.txt",
    ),
    (
        ROOT / "packages" / "downloads" / "SourceHanSansTC" / "LICENSE.txt",
        "SourceHanSansTC-LICENSE.txt",
    ),
    (
        ROOT / "schemas" / "wanxiang" / "current" / "LICENSE",
        "Wanxiang-Pinyin-LICENSE.txt",
    ),
    (
        ROOT / "third_party" / "librime" / "deps" / "include" / "COPYING.darts-clone",
        "darts-clone-COPYING.txt",
    ),
)

RIME_DATA_EXCLUDED_DIRECTORIES = {".git", ".github", "__MACOSX"}
RIME_DATA_EXCLUDED_SUFFIXES = {".jpg", ".jpeg", ".png", ".zip"}
RIME_DATA_EXCLUDED_NAMES = {
    ".gitattributes",
    ".gitignore",
    ".release-please-manifest.json",
    "CHANGELOG.md",
    "release-please-config.json",
}
WINDOWS_APP_RUNTIME_INSTALLER = "windowsappruntimeinstall-x64.exe"


def resolve_tool(name, fallbacks):
    found = shutil.which(name)
    if found:
        return Path(found)
    for fallback in fallbacks:
        path = Path(fallback)
        if path.exists():
            return path
    raise RuntimeError(f"Cannot find {name}. Install it first, then run this script again.")


def convert_to_wix_id(prefix, value):
    clean = re.sub(r"[^A-Za-z0-9_]", "_", value)
    if len(clean) > 56:
        clean = clean[:56]
    return f"{prefix}_{clean}"


def get_wix_file_id(relative_file):
    if relative_file.lower() == "fluent-pinyin-devtools.exe":
        return "DevtoolsExe"
    return convert_to_wix_id("fil", relative_file)


def add_wix_directory_xml(builder, directory_path, relative_path, component_ids, indent):
    indent_text = " " * indent
    for file_path in sorted((p for p in directory_path.iterdir() if p.is_file()), key=lambda p: p.name):
        relative_file = file_path.name if not relative_path else f"{relative_path}\\{file_path.name}"
        component_id = convert_to_wix_id("cmp", relative_file)
        file_id = get_wix_file_id(relative_file)
        component_ids.append(component_id)
        builder.append(f'{indent_text}<Component Id="{component_id}" Guid="*">\n')
        builder.append(
            f'{indent_text}  <File Id="{file_id}" Source="{escape(str(file_path))}" KeyPath="yes" />\n'
        )
        builder.append(f"{indent_text}</Component>\n")

    for child in sorted((p for p in directory_path.iterdir() if p.is_dir()), key=lambda p: p.name):
        relative_dir = child.name if not relative_path else f"{relative_path}\\{child.name}"
        directory_id = convert_to_wix_id("dir", relative_dir)
        builder.append(
            f'{indent_text}<Directory Id="{directory_id}" Name="{escape(child.name)}">\n'
        )
        add_wix_directory_xml(builder, child, relative_dir, component_ids, indent + 2)
        builder.append(f"{indent_text}</Directory>\n")


def write_wix_payload_file(payload_path, output_path):
    builder = []
    component_ids = []
    builder.append('<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">\n')
    builder.append("  <Fragment>\n")
    builder.append('    <DirectoryRef Id="INSTALLFOLDER">\n')
    add_wix_directory_xml(builder, payload_path, "", component_ids, 6)
    builder.append("    </DirectoryRef>\n")
    builder.append("  </Fragment>\n")
    builder.append("  <Fragment>\n")
    builder.append('    <ComponentGroup Id="PayloadComponents">\n')
    for component_id in component_ids:
        builder.append(f'      <ComponentRef Id="{component_id}" />\n')
    builder.append("    </ComponentGroup>\n")
    builder.append("  </Fragment>\n")
    builder.append("</Wix>\n")
    output_path.write_text("".join(builder), encoding="utf-8")


def copy_required(source_dir, destination_dir, file_name):
    source = source_dir / file_name
    if not source.exists():
        raise RuntimeError(f"Missing build artifact: {source}")
    shutil.copy2(source, destination_dir / file_name)


def copy_optional(source_dir, destination_dir, file_name):
    source = source_dir / file_name
    if source.exists():
        shutil.copy2(source, destination_dir / file_name)


def copy_directory_clean(source, destination):
    if not source.exists():
        raise RuntimeError(f"Missing directory: {source}")
    if destination.exists():
        shutil.rmtree(destination)
    shutil.copytree(source, destination)


def copy_windows_app_runtime_installer(payload_dir):
    source = ROOT / "packages" / "downloads" / WINDOWS_APP_RUNTIME_INSTALLER
    if not source.exists():
        raise RuntimeError(
            f"Missing Windows App Runtime installer: {source}. "
            "Run scripts/prepare_packages.py before packaging."
        )
    shutil.copy2(source, payload_dir / WINDOWS_APP_RUNTIME_INSTALLER)


def should_copy_rime_data(path):
    if any(part in RIME_DATA_EXCLUDED_DIRECTORIES for part in path.parts):
        return False
    if path.name in RIME_DATA_EXCLUDED_NAMES:
        return False
    return path.suffix.lower() not in RIME_DATA_EXCLUDED_SUFFIXES


def copy_rime_data_clean(source, destination):
    if not source.exists():
        raise RuntimeError(f"Missing directory: {source}")
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True, exist_ok=True)
    for path in sorted(source.rglob("*")):
        relative = path.relative_to(source)
        if not should_copy_rime_data(relative):
            continue
        target = destination / relative
        if path.is_dir():
            target.mkdir(parents=True, exist_ok=True)
        elif path.is_file():
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)


def download_notice_file(name, url, destination_dir):
    LICENSE_CACHE_DIR.mkdir(parents=True, exist_ok=True)
    cached = LICENSE_CACHE_DIR / name
    if not cached.exists():
        with urllib.request.urlopen(url, timeout=30) as response, cached.open("wb") as output:
            shutil.copyfileobj(response, output)
    shutil.copy2(cached, destination_dir / name)


def write_generated_notice(destination_dir):
    (destination_dir / "MiSans-NOTICE.txt").write_text(
        "MiSans / MiSans TC / MiSans L3 font resources are non-open-source "
        "font files from Xiaomi HyperOS font resources. They are included in "
        "this application package as redistributable runtime font resources. "
        "FluentPinyin does not relicense these font files.\n",
        encoding="utf-8",
    )
    (destination_dir / "THIRD_PARTY_SOURCE_OFFER.txt").write_text(
        "Source availability for bundled third-party binaries\n"
        "====================================================\n\n"
        "FluentPinyin redistributes prebuilt librime binaries from the upstream "
        "librime 1.16.1 Windows release. The release metadata bundled with that "
        "binary identifies these source revisions:\n\n"
        "- librime: https://github.com/rime/librime/tree/de4700e9f6b75b109910613df907965e3cbe0567\n"
        "- librime-lua: https://github.com/hchunhui/librime-lua/tree/68f9c364a2d25a04c7d4794981d7c796b05ab627\n"
        "- librime-octagram: https://github.com/lotem/librime-octagram/tree/dfcc15115788c828d9dd7b4bff68067d3ce2ffb8\n"
        "- librime-predict: https://github.com/rime/librime-predict/tree/920bd41ebf6f9bf6855d14fbe80212e54e749791\n\n"
        "librime-octagram is distributed under GPL-3.0-only terms. Its complete "
        "corresponding source is available from the upstream URL above. "
        "FluentPinyin source code is available at https://github.com/founderyang/FluentPinyin.\n",
        encoding="utf-8",
    )


def copy_third_party_licenses(payload_dir):
    notices_dir = payload_dir / "THIRD_PARTY_LICENSES"
    if notices_dir.exists():
        shutil.rmtree(notices_dir)
    notices_dir.mkdir(parents=True, exist_ok=True)

    for source, output_name in LOCAL_LICENSES:
        if source.exists():
            shutil.copy2(source, notices_dir / output_name)

    for name, url in REMOTE_LICENSES:
        download_notice_file(name, url, notices_dir)

    write_generated_notice(notices_dir)


def product_version_from_cmake():
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\(FluentPinyin VERSION ([0-9.]+)", cmake)
    if not match:
        raise RuntimeError("Cannot determine product version from CMakeLists.txt.")
    return match.group(1)


def normalized_path(path):
    return os.path.normcase(str(Path(path).resolve(strict=False)))


def read_cmake_cache_value(build_dir, key):
    cache = build_dir / "CMakeCache.txt"
    if not cache.exists():
        raise RuntimeError(f"Missing CMake cache: {cache}. Reconfigure the build first.")
    prefix = f"{key}:"
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(prefix) and "=" in line:
            return line.split("=", 1)[1].strip()
    raise RuntimeError(f"CMake cache does not contain {key}: {cache}")


def read_generated_product_version(build_dir):
    constants = build_dir / "generated" / "common" / "constants.h"
    if not constants.exists():
        raise RuntimeError(
            f"Missing generated constants: {constants}. Reconfigure the build first."
        )
    text = constants.read_text(encoding="utf-8", errors="replace")
    match = re.search(r'kProductVersion\s*=\s*L"([^"]+)"', text)
    if not match:
        raise RuntimeError(f"Unable to read generated product version from {constants}")
    return match.group(1)


def validate_build_matches_checkout(build_dir, product_version):
    configured_source = read_cmake_cache_value(build_dir, "CMAKE_HOME_DIRECTORY")
    if normalized_path(configured_source) != normalized_path(ROOT):
        raise RuntimeError(
            f"Build directory {build_dir} was configured for {configured_source}, "
            f"but the current checkout is {ROOT}. Reconfigure this build directory "
            "before packaging."
        )

    built_version = read_generated_product_version(build_dir)
    if built_version != product_version:
        raise RuntimeError(
            f"Build directory {build_dir} contains product version {built_version}, "
            f"but packaging requested {product_version}. Reconfigure and rebuild "
            "before packaging."
        )


def normalize_thumbprints(value):
    thumbprints = set()
    invalid = []
    for part in value.split(","):
        normalized = re.sub(r"[\s:-]", "", part).upper()
        if not normalized:
            continue
        if not re.fullmatch(r"[0-9A-F]{64}", normalized):
            invalid.append(part.strip() or part)
            continue
        thumbprints.add(normalized)
    if invalid:
        raise RuntimeError(
            "Updater publisher thumbprints must be SHA-256 hex strings: "
            + ", ".join(invalid)
        )
    return sorted(thumbprints)


def read_built_updater_thumbprints(build_dir):
    config = build_dir / "generated" / "updater" / "updater_config.h"
    if not config.exists():
        raise RuntimeError(
            f"Missing generated updater config: {config}. Reconfigure the build first."
        )
    text = config.read_text(encoding="utf-8")
    match = re.search(r'L"([^"]*)"', text)
    if not match:
        raise RuntimeError(f"Unable to read updater publisher thumbprints from {config}")
    return match.group(1)


def remove_if_exists(path):
    if path.is_dir():
        shutil.rmtree(path)
    elif path.exists():
        path.unlink()


def directory_size(path):
    if path.is_file():
        return path.stat().st_size
    if not path.exists():
        return 0
    return sum(child.stat().st_size for child in path.rglob("*") if child.is_file())


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as input_file:
        for chunk in iter(lambda: input_file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def payload_category(relative_path):
    parts = relative_path.parts
    if not parts:
        return "unknown"
    if parts[0] == "rime-data":
        return "rime-data"
    if parts[0] == "fonts":
        return "fonts"
    if parts[0] == "Microsoft.UI.Xaml":
        return "winui-runtime"
    if parts[0] == "THIRD_PARTY_LICENSES":
        return "licenses"
    suffix = relative_path.suffix.lower()
    if suffix in {".dll", ".exe"}:
        return "binary"
    if suffix in {".txt", ".md"}:
        return "documents"
    return "other"


def payload_resource_manifest(payload_dir):
    files = []
    category_sizes = {}
    total = 0
    for path in sorted((p for p in payload_dir.rglob("*") if p.is_file())):
        relative = path.relative_to(payload_dir)
        size = path.stat().st_size
        category = payload_category(relative)
        total += size
        category_sizes[category] = category_sizes.get(category, 0) + size
        files.append(
            {
                "path": relative.as_posix(),
                "category": category,
                "size": size,
                "sha256": sha256_file(path),
            }
        )
    return {
        "total_size": total,
        "category_sizes": dict(sorted(category_sizes.items())),
        "files": files,
    }


def write_payload_resource_manifest(payload_dir, release_dir):
    manifest = payload_resource_manifest(payload_dir)
    (release_dir / "payload-resource-manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def payload_size_report_lines(payload_dir):
    lines = []
    total = directory_size(payload_dir)
    lines.append("Payload size report:")
    lines.append(f"  total: {total / 1024 / 1024:.1f} MB")
    for child in sorted(payload_dir.iterdir(), key=lambda item: directory_size(item), reverse=True):
        size = directory_size(child)
        lines.append(f"  {child.name}: {size / 1024 / 1024:.1f} MB")

    largest_files = sorted(
        (path for path in payload_dir.rglob("*") if path.is_file()),
        key=lambda path: path.stat().st_size,
        reverse=True,
    )[:10]
    lines.append("Largest payload files:")
    for path in largest_files:
        size = path.stat().st_size
        lines.append(f"  {path.relative_to(payload_dir)}: {size / 1024 / 1024:.1f} MB")
    return lines


def write_payload_size_report(payload_dir, release_dir):
    lines = payload_size_report_lines(payload_dir)
    for line in lines:
        print(line)
    (release_dir / "payload-size-report.txt").write_text(
        "\n".join(lines) + "\n",
        encoding="utf-8",
    )


def zip_payload(payload_dir, archive_path):
    with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in payload_dir.rglob("*"):
            if path.is_file():
                archive.write(path, path.relative_to(payload_dir))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", default="build-release")
    parser.add_argument("--config", default="Release")
    parser.add_argument("--payload-dir", default="dist/FluentPinyin")
    parser.add_argument("--release-dir", default="dist/release")
    parser.add_argument("--product-version", default="")
    parser.add_argument(
        "--updater-publisher-thumbprints",
        default=os.environ.get("FP_UPDATER_PUBLISHER_THUMBPRINTS", ""),
        help="Comma-separated SHA-256 publisher certificate thumbprints compiled into the updater.",
    )
    parser.add_argument("--skip-installers", action="store_true")
    parser.add_argument("--include-zip", action="store_true")
    args = parser.parse_args()

    product_version = args.product_version.strip() or product_version_from_cmake()
    updater_publisher_thumbprints = args.updater_publisher_thumbprints.strip()
    build_dir = (ROOT / args.build_dir).resolve()
    validate_build_matches_checkout(build_dir, product_version)
    bin_dir = build_dir / "bin" / args.config
    if not bin_dir.exists():
        raise RuntimeError(f"Cannot find build output: {bin_dir}")

    payload_dir = (ROOT / args.payload_dir).resolve()
    release_dir = (ROOT / args.release_dir).resolve()
    if payload_dir.exists():
        shutil.rmtree(payload_dir)
    payload_dir.mkdir(parents=True, exist_ok=True)
    release_dir.mkdir(parents=True, exist_ok=True)

    for name in (
        "FluentPinyin-Setup.exe",
        "FluentPinyin-Uninstall.exe",
        "FluentPinyin.msi",
        "LICENSE.txt",
        "THIRD_PARTY_NOTICES.txt",
        "payload-size-report.txt",
        "payload-resource-manifest.json",
        "payload.wxs",
        "FluentPinyin.wixpdb",
    ):
        remove_if_exists(release_dir / name)

    for file_name in (
        "fluent-pinyin-tsf.dll",
        "fluent-pinyin-core.dll",
        "fluent-pinyin-ui.exe",
        "fluent-pinyin-settings.exe",
        "fluent-pinyin-updater.exe",
        "fluent-pinyin-devtools.exe",
        "fluent-pinyin-corehost.exe",
        "rime.dll",
    ):
        copy_required(bin_dir, payload_dir, file_name)

    for file_name in (
        "fluent-pinyin.ico",
        "fluent-pinyin-dark.ico",
        "fluent-pinyin-light.ico",
        "Microsoft.WindowsAppRuntime.Bootstrap.dll",
        "resources.pri",
    ):
        copy_optional(bin_dir, payload_dir, file_name)

    copy_rime_data_clean(bin_dir / "rime-data", payload_dir / "rime-data")

    source_fonts = bin_dir / "fonts"
    if not source_fonts.exists():
        source_fonts = ROOT / "assets" / "fonts"
    copy_directory_clean(source_fonts, payload_dir / "fonts")

    winui_runtime = bin_dir / "Microsoft.UI.Xaml"
    if winui_runtime.exists():
        copy_directory_clean(winui_runtime, payload_dir / "Microsoft.UI.Xaml")

    copy_windows_app_runtime_installer(payload_dir)

    (payload_dir / "README.txt").write_text(
        f"流畅拼音 {product_version}\n\n"
        "默认安装路径：C:\\Program Files\\FluentPinyin\n\n"
        "请使用 FluentPinyin.msi 安装、升级或卸载。\n"
        "设置面板的“关于”页面会从 GitHub 最新发行版下载同名 MSI 进行更新。\n",
        encoding="utf-8",
    )
    for source_name, output_name in (
        ("LICENSE", "LICENSE.txt"),
        ("THIRD_PARTY_NOTICES.md", "THIRD_PARTY_NOTICES.txt"),
    ):
        source = ROOT / source_name
        if not source.exists():
            raise RuntimeError(f"Missing required notice file: {source}")
        text = source.read_text(encoding="utf-8")
        (payload_dir / output_name).write_text(text, encoding="utf-8")

    copy_third_party_licenses(payload_dir)
    write_payload_size_report(payload_dir, release_dir)
    write_payload_resource_manifest(payload_dir, release_dir)
    payload_wxs = release_dir / "payload.wxs"
    write_wix_payload_file(payload_dir, payload_wxs)

    if args.include_zip:
        archive = release_dir / "FluentPinyin-payload.zip"
        zip_payload(payload_dir, archive)
        print(f"Archive: {archive}")

    if not args.skip_installers:
        if not updater_publisher_thumbprints:
            raise RuntimeError(
                "Release installers require --updater-publisher-thumbprints or "
                "FP_UPDATER_PUBLISHER_THUMBPRINTS. Reconfigure and rebuild the updater "
                "with the same SHA-256 signing certificate thumbprint before packaging."
            )
        built_thumbprints = read_built_updater_thumbprints(build_dir)
        expected_thumbprints = normalize_thumbprints(updater_publisher_thumbprints)
        if not expected_thumbprints:
            raise RuntimeError(
                "Release installers require at least one SHA-256 updater publisher "
                "thumbprint."
            )
        if normalize_thumbprints(built_thumbprints) != expected_thumbprints:
            raise RuntimeError(
                "Built updater publisher thumbprints do not match packaging input. "
                "Reconfigure and rebuild with -DFP_UPDATER_PUBLISHER_THUMBPRINTS "
                "before packaging."
            )
        program_files = Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
        wix = resolve_tool("wix.exe", [program_files / "WiX Toolset v7.0" / "bin" / "wix.exe"])
        product_code = "{" + str(uuid.uuid4()).upper() + "}"
        command = [
            str(wix),
            "--acceptEula",
            "wix7",
            "build",
            str(ROOT / "installer" / "fluent-pinyin.wxs"),
            str(payload_wxs),
            "-d",
            f"ProductVersion={product_version}",
            "-d",
            f"ProductCode={product_code}",
            "-arch",
            "x64",
            "-o",
            str(release_dir / "FluentPinyin.msi"),
        ]
        subprocess.run(command, cwd=ROOT, check=True)
        remove_if_exists(release_dir / "FluentPinyin.wixpdb")
        print(f"ProductCode: {product_code}")

    print(f"Payload: {payload_dir}")
    print(f"Release: {release_dir}")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
