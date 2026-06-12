# 流畅拼音

流畅拼音是面向 Windows 11 x64 的中文输入法。仓库包含源码、安装包定义和本地构建脚本；下载依赖、万象输入数据、字体和发行产物不直接提交到 Git，需要在本地构建前准备。

## 功能亮点

- 基于 TSF 的 Windows 输入法服务。
- 集成 Rime 与万象拼音数据，支持全拼和双拼。
- WinUI 3 设置面板，覆盖输入状态、候选框、外观、词库、热键和同步设置。
- 候选框支持紧凑/展开、横排/竖排、字体大小和多显示器 DPI 适配。
- 安装包内置 Windows App Runtime 依赖检查，字体使用私有加载，降低卸载残留风险。

## 构建

环境要求：

- Windows 11 x64
- Visual Studio 2022 Build Tools，包含 MSVC x64
- CMake 3.24 或更高版本
- Python 3，生成图标时需要 Pillow
- WiX Toolset 7
- GitHub CLI，用于发布发行版

如果首次使用 WiX 时提示确认 EULA，先运行：

```text
wix eula accept wix7
```

准备本地依赖：

```text
python .\scripts\check_env.py
python .\scripts\prepare_packages.py
python .\scripts\generate-icons.py
```

本地构建：

```text
cmake -S . -B .\build-release -G "Visual Studio 17 2022" -A x64
cmake --build .\build-release --config Release --parallel
```

生成本地 smoke 安装包：

```text
python .\scripts\package_release.py --skip-installers
```

正式发行安装包必须使用真实代码签名证书 thumbprint 重新配置、构建、打包和签名：

```text
cmake -S . -B .\build-release -G "Visual Studio 17 2022" -A x64 -DFP_UPDATER_PUBLISHER_THUMBPRINTS=<证书 SHA-256 thumbprint>
cmake --build .\build-release --config Release --parallel
python .\scripts\package_release.py --updater-publisher-thumbprints <同一个 SHA-256 thumbprint>
signtool sign /fd SHA256 /tr "http://timestamp.digicert.com" /td SHA256 /sha1 <证书 SHA-1 thumbprint> .\dist\release\FluentPinyin.msi
signtool verify /pa /v .\dist\release\FluentPinyin.msi
```

生成的文件位于 `dist\release`，其中签名后的 `FluentPinyin.msi` 是最终安装包。默认安装路径为 `C:\Program Files\FluentPinyin`。

也可以使用 GitHub Actions 的 `Release` 手动工作流发布正式安装包。运行前需要配置仓库 secrets：

- `WINDOWS_SIGNING_CERTIFICATE_PFX_BASE64`
- `WINDOWS_SIGNING_CERTIFICATE_PASSWORD`
- `WINDOWS_SIGNING_CERTIFICATE_SHA256`
- `WINDOWS_SIGNING_CERTIFICATE_SHA1`

签名材料配置后，触发正式发布前可先检查发布前置条件：

```text
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\check_release_prereqs.ps1 -Tag v00.00.08
```

发布完成后可验证 GitHub Release 状态、资产和线上 MSI 签名：

```text
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\verify_github_release.ps1 -Tag v00.00.08 -ExpectedPublisherSha256Thumbprint <证书 SHA-256 thumbprint> -ExpectedPublisherSha1Thumbprint <证书 SHA-1 thumbprint>
```

发布前完成本机安装后，可运行安装验证：

```text
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\install_verify.ps1 -ExpectedProductCode "{产品代码}" -RequireWanxiangPatch
```

## 更新

设置面板“关于”页面的更新按钮会从 GitHub 最新发行版下载名为 `FluentPinyin.msi` 的安装包。

## 第三方组件

流畅拼音源码使用 MIT License。详见 `LICENSE`。

打包的依赖、输入数据、字体和图标保留各自原始许可。重新分发前请阅读 `THIRD_PARTY_NOTICES.md` 和 `third_party/README.md`。
