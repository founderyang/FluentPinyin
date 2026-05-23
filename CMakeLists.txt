cmake_minimum_required(VERSION 3.24)

project(FluentPinyin VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "FluentPinyin targets Windows 11 x64 only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "FluentPinyin only supports x64 builds.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/utf-8)
endif()

set(FP_LIBRIME_ROOT "${CMAKE_SOURCE_DIR}/third_party/librime/rime/dist")
set(FP_FROST_SOURCE_DIR "${CMAKE_SOURCE_DIR}/schemas/frost/current")
set(FP_WITH_LIBRIME OFF)
if(EXISTS "${FP_LIBRIME_ROOT}/include/rime_api.h" AND EXISTS "${FP_LIBRIME_ROOT}/lib/rime.lib")
  set(FP_WITH_LIBRIME ON)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  string(TOUPPER "${config}" config_upper)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/bin/${config}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/bin/${config}")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/lib/${config}")
endforeach()

add_library(fp_common STATIC
  src/common/logging.cpp
  src/common/path_utils.cpp
)

target_include_directories(fp_common PUBLIC src)

target_compile_definitions(fp_common PUBLIC
  UNICODE
  _UNICODE
  NOMINMAX
  WIN32_LEAN_AND_MEAN
  _WIN32_WINNT=0x0A00
)

add_library(fp_core SHARED
  src/core/rime_engine.cpp
  src/core/fp_core.cpp
)

target_link_libraries(fp_core PRIVATE fp_common)
target_compile_definitions(fp_core PRIVATE FP_CORE_EXPORTS)
if(FP_WITH_LIBRIME)
  target_compile_definitions(fp_core PRIVATE FP_WITH_LIBRIME RIME_IMPORTS)
  target_include_directories(fp_core PRIVATE "${FP_LIBRIME_ROOT}/include")
  target_link_libraries(fp_core PRIVATE "${FP_LIBRIME_ROOT}/lib/rime.lib")
endif()
set_target_properties(fp_core PROPERTIES OUTPUT_NAME "fp-core")

add_library(fp_tsf SHARED
  src/tsf/class_factory.cpp
  src/tsf/dllmain.cpp
  src/tsf/registration.cpp
  src/tsf/tsf_text_service.cpp
  src/tsf/fp-tsf.def
)

target_link_libraries(fp_tsf PRIVATE
  fp_common
  fp_core
  advapi32
  gdi32
  ole32
  oleaut32
  shell32
  user32
  uuid
)

if(FP_WITH_LIBRIME)
  add_custom_command(TARGET fp_core POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FP_LIBRIME_ROOT}/lib/rime.dll"
            "$<TARGET_FILE_DIR:fp_core>/rime.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${FP_FROST_SOURCE_DIR}"
            "$<TARGET_FILE_DIR:fp_core>/rime-data"
    VERBATIM
  )
endif()

set_target_properties(fp_tsf PROPERTIES
  OUTPUT_NAME "fp-tsf"
  PREFIX ""
)

add_executable(fp_ui WIN32
  src/ui/main.cpp
)

target_link_libraries(fp_ui PRIVATE
  fp_common
  user32
  gdi32
  d2d1
  dwrite
  dcomp
  dwmapi
)

set_target_properties(fp_ui PROPERTIES OUTPUT_NAME "fp-ui")

add_executable(fp_config WIN32
  src/config/main.cpp
)

target_link_libraries(fp_config PRIVATE fp_common comdlg32 shell32 user32)
set_target_properties(fp_config PROPERTIES OUTPUT_NAME "fp-config")

add_executable(fp_updater
  src/updater/main.cpp
)

target_link_libraries(fp_updater PRIVATE fp_common wininet)
set_target_properties(fp_updater PROPERTIES OUTPUT_NAME "fp-updater")

add_executable(fp_devtools
  src/devtools/main.cpp
)

target_link_libraries(fp_devtools PRIVATE
  fp_common
  ole32
  uuid
)

set_target_properties(fp_devtools PROPERTIES OUTPUT_NAME "fp-devtools")

add_executable(fp_rime_smoke
  src/core/rime_engine.cpp
  src/devtools/rime_smoke.cpp
)

target_link_libraries(fp_rime_smoke PRIVATE fp_common)
target_compile_definitions(fp_rime_smoke PRIVATE FP_CORE_STATIC)
if(FP_WITH_LIBRIME)
  target_compile_definitions(fp_rime_smoke PRIVATE FP_WITH_LIBRIME RIME_IMPORTS)
  target_include_directories(fp_rime_smoke PRIVATE "${FP_LIBRIME_ROOT}/include")
  target_link_libraries(fp_rime_smoke PRIVATE "${FP_LIBRIME_ROOT}/lib/rime.lib")
endif()
set_target_properties(fp_rime_smoke PROPERTIES OUTPUT_NAME "fp-rime-smoke")

add_executable(fp_lexicon_import
  src/devtools/lexicon_import.cpp
)

target_link_libraries(fp_lexicon_import PRIVATE fp_common)
set_target_properties(fp_lexicon_import PROPERTIES OUTPUT_NAME "fp-lexicon-import")
