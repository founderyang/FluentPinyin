cmake_minimum_required(VERSION 3.24)

project(FluentPinyin VERSION 00.00.04 LANGUAGES CXX)

include(CTest)

if(NOT WIN32)
  message(FATAL_ERROR "FluentPinyin targets Windows 11 x64 only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "FluentPinyin only supports x64 builds.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(MSVC)
  add_compile_options(/utf-8)
endif()

set(FP_ICON_FILES
  fluent-pinyin.ico
  fluent-pinyin-dark.ico
  fluent-pinyin-light.ico
)

function(fp_copy_app_icons target_name)
  foreach(icon_file IN LISTS FP_ICON_FILES)
    add_custom_command(TARGET ${target_name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${CMAKE_SOURCE_DIR}/assets/icons/${icon_file}"
              "$<TARGET_FILE_DIR:${target_name}>/${icon_file}"
      VERBATIM
    )
  endforeach()
endfunction()

function(fp_copy_private_fonts target_name)
  add_custom_command(TARGET ${target_name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets/fonts"
            "$<TARGET_FILE_DIR:${target_name}>/fonts"
    VERBATIM
  )
endfunction()

function(fp_copy_app_assets target_name)
  fp_copy_app_icons(${target_name})
  fp_copy_private_fonts(${target_name})
endfunction()

function(fp_resolve_nuget_package out_var package_id)
  set(current_value "${${out_var}}")
  if(NOT current_value STREQUAL "")
    if(EXISTS "${current_value}")
      set(${out_var} "${current_value}" PARENT_SCOPE)
      return()
    endif()
    message(FATAL_ERROR "${out_var} points to a missing package directory: ${current_value}")
  endif()

  file(GLOB package_candidates LIST_DIRECTORIES true
       "${FP_WINUI_PACKAGE_ROOT}/${package_id}.*")
  list(SORT package_candidates COMPARE NATURAL ORDER DESCENDING)
  list(LENGTH package_candidates package_count)
  if(package_count EQUAL 0)
    message(FATAL_ERROR
            "Missing NuGet package '${package_id}' under ${FP_WINUI_PACKAGE_ROOT}. "
            "Run scripts/prepare_packages.py or set ${out_var}.")
  endif()

  list(GET package_candidates 0 resolved_package)
  set(${out_var} "${resolved_package}" CACHE PATH
      "Resolved package root for ${package_id}" FORCE)
  set(${out_var} "${resolved_package}" PARENT_SCOPE)
endfunction()

set(FP_LIBRIME_ROOT "${CMAKE_SOURCE_DIR}/third_party/librime/rime/dist")
set(FP_OPENCC_ROOT "${CMAKE_SOURCE_DIR}/third_party/librime/deps")
set(FP_OPENCC_DATA_DIR "${CMAKE_SOURCE_DIR}/third_party/librime/deps/share/opencc")
set(FP_WANXIANG_SOURCE_DIR "${CMAKE_SOURCE_DIR}/schemas/wanxiang/current")
set(FP_WITH_LIBRIME OFF)
if(EXISTS "${FP_LIBRIME_ROOT}/include/rime_api.h" AND EXISTS "${FP_LIBRIME_ROOT}/lib/rime.lib")
  set(FP_WITH_LIBRIME ON)
endif()
set(FP_WITH_OPENCC OFF)
if(EXISTS "${FP_OPENCC_ROOT}/include/opencc/opencc.h" AND EXISTS "${FP_OPENCC_ROOT}/lib/opencc.lib")
  set(FP_WITH_OPENCC ON)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

configure_file(
  "${CMAKE_SOURCE_DIR}/src/common/constants.h.in"
  "${CMAKE_BINARY_DIR}/generated/common/constants.h"
  @ONLY
)

foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  string(TOUPPER "${config}" config_upper)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/bin/${config}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/bin/${config}")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/lib/${config}")
endforeach()

add_library(fluent_pinyin_common STATIC
  src/common/encoding.cpp
  src/common/logging.cpp
  src/common/path_utils.cpp
  src/common/settings_store.cpp
)

target_include_directories(fluent_pinyin_common PUBLIC src)
target_include_directories(fluent_pinyin_common PUBLIC "${CMAKE_BINARY_DIR}/generated")

target_compile_definitions(fluent_pinyin_common PUBLIC
  UNICODE
  _UNICODE
  NOMINMAX
  WIN32_LEAN_AND_MEAN
  _WIN32_WINNT=0x0A00
)

add_library(fluent_pinyin_core SHARED
  src/core/rime_engine.cpp
  src/core/fluent_pinyin_core.cpp
)

target_link_libraries(fluent_pinyin_core PRIVATE fluent_pinyin_common)
target_compile_definitions(fluent_pinyin_core PRIVATE FP_CORE_EXPORTS)
if(FP_WITH_LIBRIME)
  target_compile_definitions(fluent_pinyin_core PRIVATE FP_WITH_LIBRIME RIME_IMPORTS)
  target_include_directories(fluent_pinyin_core PRIVATE "${FP_LIBRIME_ROOT}/include")
  target_link_libraries(fluent_pinyin_core PRIVATE "${FP_LIBRIME_ROOT}/lib/rime.lib")
endif()
if(FP_WITH_OPENCC)
  target_compile_definitions(fluent_pinyin_core PRIVATE FP_WITH_OPENCC Opencc_BUILT_AS_STATIC)
  target_include_directories(fluent_pinyin_core PRIVATE "${FP_OPENCC_ROOT}/include")
  target_link_libraries(fluent_pinyin_core PRIVATE
    "${FP_OPENCC_ROOT}/lib/opencc.lib"
    "${FP_OPENCC_ROOT}/lib/marisa.lib"
  )
endif()
set_target_properties(fluent_pinyin_core PROPERTIES OUTPUT_NAME "fluent-pinyin-core")

add_library(fluent_pinyin_tsf SHARED
  src/tsf/class_factory.cpp
  src/tsf/dllmain.cpp
  src/tsf/registration.cpp
  src/tsf/tsf_text_service.cpp
  src/tsf/fluent-pinyin-tsf.rc
  src/tsf/fluent-pinyin-tsf.def
)

target_link_libraries(fluent_pinyin_tsf PRIVATE
  fluent_pinyin_common
  fluent_pinyin_core
  advapi32
  dwmapi
  gdiplus
  gdi32
  ole32
  oleaut32
  shell32
  shlwapi
  user32
  uuid
)

if(FP_WITH_LIBRIME)
  add_custom_command(TARGET fluent_pinyin_core POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FP_LIBRIME_ROOT}/lib/rime.dll"
            "$<TARGET_FILE_DIR:fluent_pinyin_core>/rime.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${FP_WANXIANG_SOURCE_DIR}"
            "$<TARGET_FILE_DIR:fluent_pinyin_core>/rime-data"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${FP_OPENCC_DATA_DIR}"
            "$<TARGET_FILE_DIR:fluent_pinyin_core>/rime-data/opencc"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FP_WANXIANG_SOURCE_DIR}/wanxiang-lts-zh-hans.gram"
            "$<TARGET_FILE_DIR:fluent_pinyin_core>/rime-data/wanxiang-lts-zh-hans.gram"
    VERBATIM
  )
endif()

set_target_properties(fluent_pinyin_tsf PROPERTIES
  OUTPUT_NAME "fluent-pinyin-tsf"
  PREFIX ""
)

fp_copy_app_assets(fluent_pinyin_tsf)

add_executable(fluent_pinyin_ui WIN32
  src/ui/main.cpp
)

target_link_libraries(fluent_pinyin_ui PRIVATE
  fluent_pinyin_common
  ole32
  shell32
  user32
  gdi32
  d2d1
  dwrite
  dcomp
  dwmapi
  uuid
)

set_target_properties(fluent_pinyin_ui PROPERTIES OUTPUT_NAME "fluent-pinyin-ui")

set(FP_WINUI_PACKAGE_ROOT "${CMAKE_SOURCE_DIR}/packages/winui" CACHE PATH
    "Directory containing FluentPinyin WinUI NuGet packages")
set(FP_WINAPPSDK_FOUNDATION_ROOT "" CACHE PATH
    "Windows App SDK Foundation NuGet package root")
set(FP_WINAPPSDK_WINUI_ROOT "" CACHE PATH
    "Windows App SDK WinUI NuGet package root")
set(FP_WINAPPSDK_INTERACTIVE_ROOT "" CACHE PATH
    "Windows App SDK Interactive Experiences NuGet package root")
set(FP_WEBVIEW2_ROOT "" CACHE PATH
    "Microsoft WebView2 NuGet package root")
set(FP_CPPWINRT_ROOT "" CACHE PATH
    "Microsoft C++/WinRT NuGet package root")

fp_resolve_nuget_package(FP_WINAPPSDK_FOUNDATION_ROOT
                         "microsoft.windowsappsdk.foundation")
fp_resolve_nuget_package(FP_WINAPPSDK_WINUI_ROOT
                         "microsoft.windowsappsdk.winui")
fp_resolve_nuget_package(FP_WINAPPSDK_INTERACTIVE_ROOT
                         "microsoft.windowsappsdk.interactiveexperiences")
fp_resolve_nuget_package(FP_WEBVIEW2_ROOT
                         "microsoft.web.webview2")
fp_resolve_nuget_package(FP_CPPWINRT_ROOT
                         "microsoft.windows.cppwinrt")

set(FP_WINDOWS_KITS_ROOT "C:/Program Files (x86)/Windows Kits/10" CACHE PATH
    "Windows 10/11 SDK root")
set(FP_WINDOWS_SDK_VERSION "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}" CACHE STRING
    "Windows SDK version used for WinUI metadata and C++/WinRT projection")
if(FP_WINDOWS_SDK_VERSION STREQUAL "" OR
   NOT EXISTS "${FP_WINDOWS_KITS_ROOT}/UnionMetadata/${FP_WINDOWS_SDK_VERSION}/Windows.winmd")
  file(GLOB FP_WINDOWS_SDK_UNION_DIRS LIST_DIRECTORIES true
       "${FP_WINDOWS_KITS_ROOT}/UnionMetadata/10.*")
  list(SORT FP_WINDOWS_SDK_UNION_DIRS COMPARE NATURAL ORDER DESCENDING)
  list(LENGTH FP_WINDOWS_SDK_UNION_DIRS FP_WINDOWS_SDK_UNION_DIR_COUNT)
  if(FP_WINDOWS_SDK_UNION_DIR_COUNT GREATER 0)
    list(GET FP_WINDOWS_SDK_UNION_DIRS 0 FP_WINDOWS_SDK_UNION_DIR)
    get_filename_component(FP_WINDOWS_SDK_VERSION
                           "${FP_WINDOWS_SDK_UNION_DIR}" NAME)
    set(FP_WINDOWS_SDK_VERSION "${FP_WINDOWS_SDK_VERSION}" CACHE STRING
        "Windows SDK version used for WinUI metadata and C++/WinRT projection" FORCE)
  endif()
endif()

set(FP_CPPWINRT_EXE "" CACHE FILEPATH "Path to cppwinrt.exe")
if(FP_CPPWINRT_EXE STREQUAL "")
  set(FP_CPPWINRT_EXE "${FP_CPPWINRT_ROOT}/bin/cppwinrt.exe")
endif()
if(NOT EXISTS "${FP_CPPWINRT_EXE}")
  set(FP_CPPWINRT_EXE
      "${FP_WINDOWS_KITS_ROOT}/bin/${FP_WINDOWS_SDK_VERSION}/x64/cppwinrt.exe")
endif()
set(FP_WINDOWS_SDK_UNION_METADATA
    "${FP_WINDOWS_KITS_ROOT}/UnionMetadata/${FP_WINDOWS_SDK_VERSION}/Windows.winmd")
set(FP_WINDOWS_SDK_CPPWINRT_INCLUDE
    "${FP_WINDOWS_KITS_ROOT}/Include/${FP_WINDOWS_SDK_VERSION}/cppwinrt")
foreach(required_path IN ITEMS
    "${FP_CPPWINRT_EXE}"
    "${FP_WINDOWS_SDK_UNION_METADATA}"
    "${FP_WINDOWS_SDK_CPPWINRT_INCLUDE}")
  if(NOT EXISTS "${required_path}")
    message(FATAL_ERROR "Missing WinUI build dependency: ${required_path}")
  endif()
endforeach()
file(GLOB FP_WINAPPSDK_FOUNDATION_WINMDS
  "${FP_WINAPPSDK_FOUNDATION_ROOT}/metadata/*.winmd"
)
file(GLOB FP_WINAPPSDK_INTERACTIVE_WINMDS
  "${FP_WINAPPSDK_INTERACTIVE_ROOT}/metadata/10.0.18362.0/*.winmd"
)
set(FP_WINUI_WINMD_FILES
  "${FP_WINDOWS_SDK_UNION_METADATA}"
  "${FP_WEBVIEW2_ROOT}/lib/Microsoft.Web.WebView2.Core.winmd"
  ${FP_WINAPPSDK_FOUNDATION_WINMDS}
  ${FP_WINAPPSDK_INTERACTIVE_WINMDS}
  "${FP_WINAPPSDK_WINUI_ROOT}/metadata/Microsoft.UI.Text.winmd"
  "${FP_WINAPPSDK_WINUI_ROOT}/metadata/Microsoft.UI.Xaml.winmd"
)
set(FP_WINUI_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/winui")
set(FP_WINUI_PROJECTION_STAMP "${FP_WINUI_GENERATED_DIR}/projection.stamp")
add_custom_command(
  OUTPUT "${FP_WINUI_PROJECTION_STAMP}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${FP_WINUI_GENERATED_DIR}"
  COMMAND "${FP_CPPWINRT_EXE}" -input ${FP_WINUI_WINMD_FILES} -output "${FP_WINUI_GENERATED_DIR}"
  COMMAND ${CMAKE_COMMAND} -E touch "${FP_WINUI_PROJECTION_STAMP}"
  DEPENDS ${FP_WINUI_WINMD_FILES}
  VERBATIM
)
add_custom_target(fluent_pinyin_winui_projection DEPENDS "${FP_WINUI_PROJECTION_STAMP}")

add_executable(fluent_pinyin_settings WIN32
  src/config_winui/main.cpp
  src/sync/sync_service.cpp
  src/config_winui/fluent-pinyin-settings.rc
)

add_dependencies(fluent_pinyin_settings fluent_pinyin_winui_projection)
target_include_directories(fluent_pinyin_settings PRIVATE
  "${FP_WINUI_GENERATED_DIR}"
  "${FP_WINDOWS_SDK_CPPWINRT_INCLUDE}"
  "${FP_WINAPPSDK_FOUNDATION_ROOT}/include"
  "${FP_WINAPPSDK_WINUI_ROOT}/include"
)
target_link_libraries(fluent_pinyin_settings PRIVATE
  fluent_pinyin_common
  "${FP_WINAPPSDK_FOUNDATION_ROOT}/lib/native/x64/Microsoft.WindowsAppRuntime.Bootstrap.lib"
  bcrypt
  crypt32
  taskschd
  comsuppw
  winhttp
  ole32
  runtimeobject
  comdlg32
  shell32
  user32
  windowsapp
)
set_target_properties(fluent_pinyin_settings PROPERTIES OUTPUT_NAME "fluent-pinyin-settings")

fp_copy_app_assets(fluent_pinyin_settings)
add_custom_command(TARGET fluent_pinyin_settings POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${FP_WINAPPSDK_FOUNDATION_ROOT}/runtimes/win-x64/native/Microsoft.WindowsAppRuntime.Bootstrap.dll"
          "$<TARGET_FILE_DIR:fluent_pinyin_settings>/Microsoft.WindowsAppRuntime.Bootstrap.dll"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${FP_WINAPPSDK_WINUI_ROOT}/runtimes-framework/win-x64/native/Microsoft.UI.Xaml.Controls.pri"
          "$<TARGET_FILE_DIR:fluent_pinyin_settings>/resources.pri"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${FP_WINAPPSDK_WINUI_ROOT}/runtimes-framework/win-x64/native/Microsoft.UI.Xaml"
          "$<TARGET_FILE_DIR:fluent_pinyin_settings>/Microsoft.UI.Xaml"
  VERBATIM
)

add_executable(fluent_pinyin_updater WIN32
  src/updater/main.cpp
)

target_link_libraries(fluent_pinyin_updater PRIVATE fluent_pinyin_common wininet wintrust crypt32)
set_target_properties(fluent_pinyin_updater PROPERTIES OUTPUT_NAME "fluent-pinyin-updater")

add_executable(fluent_pinyin_devtools WIN32
  src/devtools/main.cpp
)

target_link_libraries(fluent_pinyin_devtools PRIVATE
  fluent_pinyin_common
  fluent_pinyin_core
  advapi32
  gdi32
  ole32
  oleaut32
  taskschd
  uuid
)

set_target_properties(fluent_pinyin_devtools PROPERTIES OUTPUT_NAME "fluent-pinyin-devtools")

if(BUILD_TESTING)
  add_executable(common_unit
    tests/common_unit.cpp
  )
  target_link_libraries(common_unit PRIVATE fluent_pinyin_common)

  add_executable(core_unit
    tests/core_unit.cpp
  )
  target_link_libraries(core_unit PRIVATE fluent_pinyin_common fluent_pinyin_core)

  add_executable(sync_package_integrity
    tests/sync_package_integrity.cpp
    src/sync/sync_service.cpp
  )
  target_link_libraries(sync_package_integrity PRIVATE
    fluent_pinyin_common
    bcrypt
    crypt32
    taskschd
    comsuppw
    winhttp
    ole32
    user32
  )

  add_executable(sync_settings
    tests/sync_settings.cpp
    src/sync/sync_service.cpp
  )
  target_link_libraries(sync_settings PRIVATE
    fluent_pinyin_common
    bcrypt
    crypt32
    taskschd
    comsuppw
    winhttp
    ole32
    user32
  )

  add_test(
    NAME smoke_release
    COMMAND powershell
            -NoProfile
            -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/tests/smoke.ps1"
            -BinDir "$<TARGET_FILE_DIR:fluent_pinyin_devtools>"
            -GeneratedDir "${CMAKE_BINARY_DIR}/generated"
            -ExpectedVersion "${PROJECT_VERSION}"
  )
  add_test(
    NAME common_unit
    COMMAND common_unit
  )
  add_test(
    NAME core_unit
    COMMAND core_unit
  )
  add_test(
    NAME sync_package_integrity
    COMMAND sync_package_integrity "${CMAKE_BINARY_DIR}/sync-package-integrity"
  )
  add_test(
    NAME sync_settings
    COMMAND sync_settings "${CMAKE_BINARY_DIR}/sync-settings"
  )
endif()
