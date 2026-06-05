Unicode true
ManifestDPIAware true
RequestExecutionLevel admin
SilentInstall normal
SilentUnInstall normal
AutoCloseWindow true

!ifndef PRODUCT_VERSION
!define PRODUCT_VERSION "00.00.01"
!endif

!ifndef PAYLOAD_DIR
!define PAYLOAD_DIR "..\dist\FluentPinyin"
!endif

!ifndef OUTPUT_DIR
!define OUTPUT_DIR "..\dist\release"
!endif

!define PRODUCT_NAME "FluentPinyin"
!define INSTALL_DIR "$PROGRAMFILES64\FluentPinyin"

Name "FluentPinyin"
OutFile "${OUTPUT_DIR}\FluentPinyin-Setup.exe"
InstallDir "${INSTALL_DIR}"
InstallDirRegKey HKLM "Software\FluentPinyin" "InstallDir"
ShowInstDetails show
ShowUninstDetails show

Section "Install"
  SetShellVarContext all
  SetRegView 64
  SetOutPath "$INSTDIR"
  RMDir /r "$INSTDIR"
  CreateDirectory "$INSTDIR"
  SetOutPath "$INSTDIR"
  File /r "${PAYLOAD_DIR}\*"
  WriteRegStr HKLM "Software\FluentPinyin" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FluentPinyin" "DisplayName" "FluentPinyin"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FluentPinyin" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FluentPinyin" "Publisher" "FluentPinyin"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FluentPinyin" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FluentPinyin" "UninstallString" "$INSTDIR\FluentPinyin-Uninstall.exe"
  WriteUninstaller "$INSTDIR\FluentPinyin-Uninstall.exe"
  ExecWait '"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\scripts\install-fonts.ps1" -SourceFontDir "$INSTDIR\fonts"'
  ExecWait '"$SYSDIR\regsvr32.exe" /s "$INSTDIR\fluent-pinyin-tsf.dll"'
  IfFileExists "$INSTDIR\fluent-pinyin-devtools.exe" 0 +2
    ExecWait '"$INSTDIR\fluent-pinyin-devtools.exe" activate-session'
SectionEnd

Section "Uninstall"
  SetShellVarContext all
  SetRegView 64
  IfFileExists "$INSTDIR\fluent-pinyin-devtools.exe" 0 +2
    ExecWait '"$INSTDIR\fluent-pinyin-devtools.exe" activate-ms-pinyin-session'
  IfFileExists "$INSTDIR\fluent-pinyin-tsf.dll" 0 +2
    ExecWait '"$SYSDIR\regsvr32.exe" /u /s "$INSTDIR\fluent-pinyin-tsf.dll"'
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FluentPinyin"
  DeleteRegKey HKLM "Software\FluentPinyin"
  RMDir /r "$INSTDIR"
SectionEnd
