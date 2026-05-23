#include "common/constants.h"
#include "common/logging.h"
#include "common/path_utils.h"

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace {

constexpr int kButtonWidth = 224;
constexpr int kButtonHeight = 38;
constexpr int kMargin = 24;
constexpr int kGap = 10;
constexpr int kIdImportLexicon = 1001;
constexpr int kIdOpenRimeDir = 1002;
constexpr int kIdCheckUpdates = 1003;
constexpr int kIdRedeploy = 1004;
constexpr int kIdDownloadUpdates = 1006;
constexpr int kIdStatus = 2001;

std::wstring GetModuleDirectory() {
  std::wstring buffer(32768, L'\0');
  const DWORD length =
      GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (length == 0 || length >= buffer.size()) {
    return std::filesystem::current_path().wstring();
  }

  buffer.resize(length);
  return std::filesystem::path(buffer).parent_path().wstring();
}

std::filesystem::path SiblingExe(std::wstring_view name) {
  return std::filesystem::path(GetModuleDirectory()) / std::wstring(name);
}

std::wstring QuoteArg(const std::filesystem::path& path) {
  return L"\"" + path.wstring() + L"\"";
}

void SetStatus(HWND window, std::wstring_view text) {
  SetDlgItemTextW(window, kIdStatus, std::wstring(text).c_str());
}

void ShowMessage(HWND window, std::wstring_view message, UINT icon) {
  MessageBoxW(window,
              std::wstring(message).c_str(),
              std::wstring(fp::kProductName).c_str(),
              MB_OK | icon);
}

std::wstring Utf8ToWide(std::string_view value) {
  if (value.empty()) {
    return {};
  }

  const int required = MultiByteToWideChar(CP_UTF8,
                                           0,
                                           value.data(),
                                           static_cast<int>(value.size()),
                                           nullptr,
                                           0);
  if (required <= 0) {
    return {};
  }

  std::wstring result(static_cast<size_t>(required), L'\0');
  MultiByteToWideChar(CP_UTF8,
                      0,
                      value.data(),
                      static_cast<int>(value.size()),
                      result.data(),
                      required);
  return result;
}

bool RunProcessAndWait(HWND window,
                       const std::filesystem::path& exe_path,
                       std::wstring parameters,
                       const wchar_t* success_message) {
  if (!std::filesystem::exists(exe_path)) {
    ShowMessage(window, L"\u627E\u4E0D\u5230\u7EC4\u4EF6\uFF1A" + exe_path.wstring(), MB_ICONERROR);
    return false;
  }

  SECURITY_ATTRIBUTES security{};
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;

  HANDLE read_pipe = nullptr;
  HANDLE write_pipe = nullptr;
  if (!CreatePipe(&read_pipe, &write_pipe, &security, 0)) {
    ShowMessage(window, L"\u65E0\u6CD5\u521B\u5EFA\u8F93\u51FA\u7BA1\u9053\u3002", MB_ICONERROR);
    return false;
  }
  SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

  std::wstring command_line = QuoteArg(exe_path);
  if (!parameters.empty()) {
    command_line += L" ";
    command_line += parameters;
  }

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdOutput = write_pipe;
  startup.hStdError = write_pipe;
  startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

  PROCESS_INFORMATION process{};
  std::wstring mutable_command_line = command_line;
  if (!CreateProcessW(nullptr,
                      mutable_command_line.data(),
                      nullptr,
                      nullptr,
                      TRUE,
                      CREATE_NO_WINDOW,
                      nullptr,
                      nullptr,
                      &startup,
                      &process)) {
    CloseHandle(read_pipe);
    CloseHandle(write_pipe);
    ShowMessage(window, L"\u542F\u52A8\u5931\u8D25\uFF1A" + exe_path.wstring(), MB_ICONERROR);
    return false;
  }

  CloseHandle(write_pipe);
  SetStatus(window, L"\u6B63\u5728\u6267\u884C...");

  std::string output;
  char buffer[4096]{};
  DWORD bytes_read = 0;
  while (ReadFile(read_pipe, buffer, sizeof(buffer), &bytes_read, nullptr) &&
         bytes_read > 0) {
    output.append(buffer, buffer + bytes_read);
  }

  WaitForSingleObject(process.hProcess, INFINITE);
  DWORD exit_code = 1;
  GetExitCodeProcess(process.hProcess, &exit_code);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  CloseHandle(read_pipe);

  const std::wstring output_text = Utf8ToWide(output);
  if (exit_code != 0) {
    std::wstring message =
        L"\u64CD\u4F5C\u5931\u8D25\uFF0C\u9000\u51FA\u7801\uFF1A" +
        std::to_wstring(exit_code);
    if (!output_text.empty()) {
      message += L"\n\n";
      message += output_text;
    }
    ShowMessage(window, message, MB_ICONERROR);
    SetStatus(window, L"\u64CD\u4F5C\u5931\u8D25\u3002");
    return false;
  }

  SetStatus(window, success_message);
  ShowMessage(window, success_message, MB_ICONINFORMATION);
  return true;
}

std::optional<std::filesystem::path> PickRimeDictionary(HWND window) {
  wchar_t file_name[MAX_PATH]{};
  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = window;
  ofn.lpstrFilter =
      L"RIME \u8BCD\u5178 (*.dict.yaml)\0*.dict.yaml\0YAML \u6587\u4EF6 (*.yaml)\0*.yaml\0\u6240\u6709\u6587\u4EF6 (*.*)\0*.*\0";
  ofn.lpstrFile = file_name;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  ofn.lpstrTitle = L"\u5BFC\u5165 RIME \u8BCD\u5178";

  if (!GetOpenFileNameW(&ofn)) {
    return std::nullopt;
  }
  return std::filesystem::path(file_name);
}

void OpenDirectory(HWND window, const std::filesystem::path& path) {
  fp::EnsureDirectory(path);
  const auto result =
      reinterpret_cast<INT_PTR>(ShellExecuteW(window,
                                              L"open",
                                              path.c_str(),
                                              nullptr,
                                              nullptr,
                                              SW_SHOWNORMAL));
  if (result <= 32) {
    ShowMessage(window, L"\u65E0\u6CD5\u6253\u5F00\u76EE\u5F55\uFF1A" + path.wstring(), MB_ICONERROR);
  }
}

void ImportLexicon(HWND window) {
  const auto selected = PickRimeDictionary(window);
  if (!selected) {
    return;
  }

  const auto importer = SiblingExe(L"fp-lexicon-import.exe");
  const auto user_dir = fp::GetFpRoamingDataPath() / L"Rime";
  const std::wstring parameters = QuoteArg(*selected) + L" " + QuoteArg(user_dir);
  if (RunProcessAndWait(window,
                        importer,
                        parameters,
                        L"\u8BCD\u5E93\u5DF2\u5BFC\u5165\u3002")) {
    SetStatus(window, L"\u8BCD\u5E93\u5DF2\u5BFC\u5165\uFF0C\u8BF7\u91CD\u65B0\u90E8\u7F72 RIME\u3002");
  }
}

void CheckUpdates(HWND window) {
  RunProcessAndWait(window,
                    SiblingExe(L"fp-updater.exe"),
                    L"check",
                    L"\u66F4\u65B0\u68C0\u67E5\u5DF2\u5B8C\u6210\u3002");
}

void DownloadUpdates(HWND window) {
  RunProcessAndWait(window,
                    SiblingExe(L"fp-updater.exe"),
                    L"download",
                    L"\u66F4\u65B0\u5305\u5DF2\u4E0B\u8F7D\u3002");
}

void RedeployRime(HWND window) {
  RunProcessAndWait(window,
                    SiblingExe(L"fp-rime-smoke.exe"),
                    L"nihao",
                    L"RIME \u5DF2\u91CD\u65B0\u90E8\u7F72\u5E76\u901A\u8FC7\u6D4B\u8BD5\u3002");
}

void CreateChildControls(HWND window) {
  HFONT message_font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

  auto make_static = [&](const wchar_t* text, int x, int y, int width, int height, int id) {
    HWND control = CreateWindowExW(0,
                                   L"STATIC",
                                   text,
                                   WS_CHILD | WS_VISIBLE,
                                   x,
                                   y,
                                   width,
                                   height,
                                   window,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   GetModuleHandleW(nullptr),
                                   nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(message_font), TRUE);
    return control;
  };

  auto make_button = [&](const wchar_t* text, int x, int y, int id) {
    HWND control = CreateWindowExW(0,
                                   L"BUTTON",
                                   text,
                                   WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                   x,
                                   y,
                                   kButtonWidth,
                                   kButtonHeight,
                                   window,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   GetModuleHandleW(nullptr),
                                   nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(message_font), TRUE);
    return control;
  };

  make_static(L"\u6D41\u7545\u62FC\u97F3", kMargin, 18, 260, 28, -1);
  make_static(L"\u5168\u62FC\u8F93\u5165\u4E0E\u8BCD\u5E93\u7BA1\u7406", kMargin, 50, 420, 24, -1);

  int y = 92;
  make_button(L"\u5BFC\u5165\u8BCD\u5E93", kMargin, y, kIdImportLexicon);
  y += kButtonHeight + kGap;
  make_button(L"\u91CD\u65B0\u90E8\u7F72", kMargin, y, kIdRedeploy);
  y += kButtonHeight + kGap;
  make_button(L"\u68C0\u67E5\u66F4\u65B0", kMargin, y, kIdCheckUpdates);
  y += kButtonHeight + kGap;
  make_button(L"\u4E0B\u8F7D\u66F4\u65B0", kMargin, y, kIdDownloadUpdates);
  y += kButtonHeight + kGap;
  make_button(L"\u6253\u5F00\u7528\u6237\u76EE\u5F55", kMargin, y, kIdOpenRimeDir);

  make_static(L"\u5C31\u7EEA\u3002", kMargin, 344, 430, 48, kIdStatus);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_CREATE:
      CreateChildControls(window);
      return 0;
    case WM_COMMAND:
      switch (LOWORD(wparam)) {
        case kIdImportLexicon:
          ImportLexicon(window);
          return 0;
        case kIdOpenRimeDir:
          OpenDirectory(window, fp::GetFpRoamingDataPath() / L"Rime");
          return 0;
        case kIdCheckUpdates:
          CheckUpdates(window);
          return 0;
        case kIdDownloadUpdates:
          DownloadUpdates(window);
          return 0;
        case kIdRedeploy:
          RedeployRime(window);
          return 0;
        default:
          break;
      }
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance,
                      HINSTANCE previous,
                      LPWSTR command_line,
                      int show_command) {
  (void)previous;
  (void)command_line;

  fp::LogInfo(L"config", L"fp-config opened.");

  const wchar_t* class_name = L"FluentPinyinConfigWindow";
  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.lpfnWndProc = WindowProc;
  window_class.hInstance = instance;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  window_class.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
  window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  window_class.lpszClassName = class_name;

  RegisterClassExW(&window_class);

  HWND window = CreateWindowExW(0,
                                class_name,
                                std::wstring(fp::kProductName).c_str(),
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                500,
                                450,
                                nullptr,
                                nullptr,
                                instance,
                                nullptr);
  if (window == nullptr) {
    return 1;
  }

  ShowWindow(window, show_command);
  UpdateWindow(window);

  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  return 0;
}
