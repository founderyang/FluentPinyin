# Third-Party Components and Resources

This repository does not commit downloaded binary packages, fonts, bundled
input data, or release artifacts. Run `scripts/prepare_packages.py` locally to
download the resources required for building an installer.

## Open-Source Components

- librime 1.16.1: https://github.com/rime/librime
- OpenCC: https://github.com/BYVoid/OpenCC
- Windows App SDK 1.8: https://github.com/microsoft/WindowsAppSDK
- C++/WinRT 3.0: https://github.com/microsoft/cppwinrt
- Fluent UI System Icons: https://github.com/microsoft/fluentui-system-icons
- Source Han Sans 2.005R: https://github.com/adobe-fonts/source-han-sans
- Plangothic Project: https://github.com/Fitzgerald-Porthmouth-Koenigsegg/Plangothic_Project
- Wanxiang Pinyin 15.11.1: https://github.com/amzxyz/rime-wanxiang
- RIME-LMDG LTS: https://github.com/amzxyz/RIME-LMDG

See `../THIRD_PARTY_NOTICES.md` for license identifiers, attribution, and
redistribution notes.

## Bundled Resources

- MiSans / MiSans TC / MiSans L3: non-open-source font resources allowed for
  redistribution in the application package, source https://hyperos.mi.com/font
- App icons: generated from bundled fonts by `scripts/generate-icons.py`

## Local Paths

- `packages/`: NuGet packages and downloads, ignored by Git
- `third_party/librime/`: extracted librime and dependency packages, ignored by Git
- `schemas/wanxiang/current/`: extracted input data, ignored by Git
- `assets/fonts/`: downloaded fonts used for runtime display and installer
  packaging, ignored by Git
