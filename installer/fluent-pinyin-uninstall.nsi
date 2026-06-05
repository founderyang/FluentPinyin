Unicode true
ManifestDPIAware true
RequestExecutionLevel admin
SilentInstall normal
AutoCloseWindow true

!ifndef OUTPUT_DIR
!define OUTPUT_DIR "..\dist\release"
!endif

!define INSTALL_DIR "$PROGRAMFILES64\FluentPinyin"

Name "FluentPinyin Uninstall"
OutFile "${OUTPUT_DIR}\FluentPinyin-Uninstall.exe"
InstallDir "${INSTALL_DIR}"
InstallDirRegKey HKLM "Software\FluentPinyin" "InstallDir"
ShowInstDetails show

Section "Remove FluentPinyin"
  SetShellVarContext all
  SetRegView 64
  ReadRegStr $INSTDIR HKLM "Software\FluentPinyin" "InstallDir"
  StrCmp $INSTDIR "" 0 +2
    StrCpy $INSTDIR "${INSTALL_DIR}"
  IfFileExists "$INSTDIR\fluent-pinyin-devtools.exe" 0 +2
    ExecWait '"$INSTDIR\fluent-pinyin-devtools.exe" activate-ms-pinyin-session'
  IfFileExists "$INSTDIR\fluent-pinyin-tsf.dll" 0 +2
    ExecWait '"$SYSDIR\regsvr32.exe" /u /s "$INSTDIR\fluent-pinyin-tsf.dll"'
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FluentPinyin"
  DeleteRegKey HKLM "Software\FluentPinyin"
  RMDir /r "$INSTDIR"
SectionEnd
