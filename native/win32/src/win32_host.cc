#include "win32_host.h"
#include "../resource.h"

#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

namespace {

constexpr wchar_t kOwnerClassName[] = L"SelectionForward.NativeHost";
constexpr wchar_t kIndicatorClassName[] = L"SelectionForward.Indicator";
constexpr wchar_t kShortcutClassName[] = L"SelectionForward.ShortcutCapture";
constexpr wchar_t kTrayTooltip[] = L"Selection Forward";
constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"SelectionForward";

constexpr UINT kTrayIconId = 1;
constexpr UINT kTrayCallbackMessage = WM_APP + 1;
constexpr UINT kShowIndicatorMessage = WM_APP + 2;
constexpr UINT kHideIndicatorMessage = WM_APP + 3;
constexpr UINT kUpdateTrayMessage = WM_APP + 4;
constexpr UINT kStopMessage = WM_APP + 5;
constexpr UINT kRegisterShortcutMessage = WM_APP + 6;
constexpr UINT_PTR kHoverTimerId = 1;
constexpr int kHotkeyId = 0x5346;
constexpr int kHotkeyProbeId = 0x5347;

constexpr UINT kCommandToggleEnabled = 1001;
constexpr UINT kCommandImmediate = 1100;
constexpr UINT kCommandIcon = 1101;
constexpr UINT kCommandDot = 1102;
constexpr UINT kCommandCtrl = 1103;
constexpr UINT kCommandAlt = 1104;
constexpr UINT kCommandShift = 1105;
constexpr UINT kCommandCustom = 1106;
constexpr UINT kCommandToggleAutoStart = 1200;
constexpr UINT kCommandToggleHover = 1201;
constexpr UINT kCommandSetShortcut = 1300;
constexpr UINT kCommandChooseIcon = 1301;
constexpr UINT kCommandDefaultIcon = 1302;
constexpr UINT kCommandExit = 1400;
constexpr UINT kCommandIconSize16 = 1500;
constexpr UINT kCommandIconSize20 = 1501;
constexpr UINT kCommandIconSize24 = 1502;
constexpr UINT kCommandIconSize32 = 1503;
constexpr UINT kCommandIconSize40 = 1504;
constexpr UINT kCommandDotSize8 = 1510;
constexpr UINT kCommandDotSize12 = 1511;
constexpr UINT kCommandDotSize16 = 1512;
constexpr UINT kCommandDotSize20 = 1513;
constexpr UINT kCommandDotSize24 = 1514;
constexpr UINT kShortcutSaveButton = 1600;
constexpr UINT kShortcutCancelButton = 1601;

constexpr COLORREF kIndicatorColor = RGB(31, 111, 235);

Win32Host* GetHost(HWND window) {
  return reinterpret_cast<Win32Host*>(GetWindowLongPtrW(window, GWLP_USERDATA));
}

void AttachHost(HWND window, LPARAM lparam) {
  auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
  SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
}

std::wstring QuoteExecutable(const std::wstring& path) {
  return L"\"" + path + L"\"";
}

std::string TrimAscii(std::string value) {
  const auto not_space = [](unsigned char character) { return !std::isspace(character); };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
  value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
  return value;
}

std::string UpperAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
    return static_cast<char>(std::toupper(character));
  });
  return value;
}

struct ParsedHotkey {
  bool ok = false;
  UINT modifiers = 0;
  UINT virtual_key = 0;
  std::string normalized;
};

std::string VirtualKeyName(UINT virtual_key) {
  if (virtual_key >= 'A' && virtual_key <= 'Z') {
    return std::string(1, static_cast<char>(virtual_key));
  }
  if (virtual_key >= '0' && virtual_key <= '9') {
    return std::string(1, static_cast<char>(virtual_key));
  }
  if (virtual_key >= VK_F1 && virtual_key <= VK_F24) {
    return "F" + std::to_string(virtual_key - VK_F1 + 1);
  }

  switch (virtual_key) {
    case VK_ESCAPE: return "Escape";
    case VK_TAB: return "Tab";
    case VK_SPACE: return "Space";
    case VK_RETURN: return "Enter";
    case VK_BACK: return "Backspace";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "PageUp";
    case VK_NEXT: return "PageDown";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_SNAPSHOT: return "PrintScreen";
    case VK_PAUSE: return "Pause";
    default: return {};
  }
}

UINT ParseVirtualKey(const std::string& token) {
  const std::string upper = UpperAscii(token);
  if (upper.size() == 1 &&
      ((upper[0] >= 'A' && upper[0] <= 'Z') || (upper[0] >= '0' && upper[0] <= '9'))) {
    return static_cast<UINT>(upper[0]);
  }
  if (upper.size() >= 2 && upper[0] == 'F') {
    const int function_number = std::atoi(upper.c_str() + 1);
    if (function_number >= 1 && function_number <= 24) {
      return VK_F1 + function_number - 1;
    }
  }

  if (upper == "ESC" || upper == "ESCAPE") return VK_ESCAPE;
  if (upper == "TAB") return VK_TAB;
  if (upper == "SPACE") return VK_SPACE;
  if (upper == "ENTER" || upper == "RETURN") return VK_RETURN;
  if (upper == "BACKSPACE") return VK_BACK;
  if (upper == "INSERT") return VK_INSERT;
  if (upper == "DELETE" || upper == "DEL") return VK_DELETE;
  if (upper == "HOME") return VK_HOME;
  if (upper == "END") return VK_END;
  if (upper == "PAGEUP" || upper == "PGUP") return VK_PRIOR;
  if (upper == "PAGEDOWN" || upper == "PGDN") return VK_NEXT;
  if (upper == "UP") return VK_UP;
  if (upper == "DOWN") return VK_DOWN;
  if (upper == "LEFT") return VK_LEFT;
  if (upper == "RIGHT") return VK_RIGHT;
  if (upper == "PRINTSCREEN") return VK_SNAPSHOT;
  if (upper == "PAUSE") return VK_PAUSE;
  return 0;
}

ParsedHotkey ParseHotkey(const std::string& shortcut) {
  ParsedHotkey parsed;
  std::vector<std::string> tokens;
  std::stringstream stream(shortcut);
  std::string token;
  while (std::getline(stream, token, '+')) {
    token = TrimAscii(token);
    if (token.empty()) {
      return parsed;
    }
    tokens.push_back(token);
  }
  if (tokens.size() < 2) {
    return parsed;
  }

  std::string key_name;
  for (const std::string& raw_token : tokens) {
    const std::string upper = UpperAscii(raw_token);
    if (upper == "CTRL" || upper == "CONTROL") {
      parsed.modifiers |= MOD_CONTROL;
    } else if (upper == "ALT") {
      parsed.modifiers |= MOD_ALT;
    } else if (upper == "SHIFT") {
      parsed.modifiers |= MOD_SHIFT;
    } else if (upper == "WIN" || upper == "WINDOWS" || upper == "LWIN" || upper == "RWIN") {
      parsed.modifiers |= MOD_WIN;
    } else {
      if (parsed.virtual_key != 0) {
        return ParsedHotkey{};
      }
      parsed.virtual_key = ParseVirtualKey(raw_token);
      key_name = VirtualKeyName(parsed.virtual_key);
    }
  }

  if (parsed.modifiers == 0 || parsed.virtual_key == 0 || key_name.empty()) {
    return ParsedHotkey{};
  }

  if (parsed.modifiers & MOD_CONTROL) parsed.normalized += "Ctrl+";
  if (parsed.modifiers & MOD_ALT) parsed.normalized += "Alt+";
  if (parsed.modifiers & MOD_SHIFT) parsed.normalized += "Shift+";
  if (parsed.modifiers & MOD_WIN) parsed.normalized += "Win+";
  parsed.normalized += key_name;
  parsed.ok = true;
  return parsed;
}

std::string CaptureHotkey(UINT virtual_key) {
  if (virtual_key == VK_CONTROL || virtual_key == VK_LCONTROL || virtual_key == VK_RCONTROL ||
      virtual_key == VK_MENU || virtual_key == VK_LMENU || virtual_key == VK_RMENU ||
      virtual_key == VK_SHIFT || virtual_key == VK_LSHIFT || virtual_key == VK_RSHIFT ||
      virtual_key == VK_LWIN || virtual_key == VK_RWIN) {
    return {};
  }

  std::string shortcut;
  if (GetKeyState(VK_CONTROL) & 0x8000) shortcut += "Ctrl+";
  if (GetKeyState(VK_MENU) & 0x8000) shortcut += "Alt+";
  if (GetKeyState(VK_SHIFT) & 0x8000) shortcut += "Shift+";
  if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) shortcut += "Win+";
  if (shortcut.empty()) {
    return {};
  }

  const std::string key_name = VirtualKeyName(virtual_key);
  return key_name.empty() ? std::string{} : shortcut + key_name;
}

std::wstring Utf8ToWideLocal(const std::string& value) {
  if (value.empty()) return {};
  const int size = MultiByteToWideChar(
      CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
  std::wstring result(size, L'\0');
  MultiByteToWideChar(
      CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
  return result;
}

std::string WideToUtf8(const std::wstring& value) {
  if (value.empty()) return {};
  const int size = WideCharToMultiByte(
      CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  std::string result(size, '\0');
  WideCharToMultiByte(CP_UTF8,
                      0,
                      value.data(),
                      static_cast<int>(value.size()),
                      result.data(),
                      size,
                      nullptr,
                      nullptr);
  return result;
}

}  // namespace

struct Win32Host::IndicatorRequest {
  int x;
  int y;
  std::string style;
  int size;
  std::string icon_path;
  bool hover_enabled;
  unsigned int hover_delay_ms;
};

struct Win32Host::TrayStateRequest {
  bool enabled;
  bool auto_start;
  std::string trigger_mode;
  std::string indicator_action;
  int icon_size;
  int dot_size;
  std::string icon_path;
  std::string custom_shortcut;
};

struct Win32Host::ShortcutRequest {
  std::string shortcut;
  ShortcutResult result;
};

struct Win32Host::EventPayload {
  std::string type;
  std::string value;
};

Win32Host::Win32Host(napi_env env, napi_value callback) : env_(env) {
  napi_value resource_name = nullptr;
  if (napi_create_string_utf8(env_, "selection-forward-win32", NAPI_AUTO_LENGTH, &resource_name) !=
      napi_ok) {
    return;
  }

  if (napi_create_threadsafe_function(env_,
                                      callback,
                                      nullptr,
                                      resource_name,
                                      64,
                                      1,
                                      nullptr,
                                      nullptr,
                                      this,
                                      CallJs,
                                      &threadsafe_function_) != napi_ok) {
    threadsafe_function_ = nullptr;
    return;
  }

  napi_unref_threadsafe_function(env_, threadsafe_function_);
}

Win32Host::~Win32Host() {
  Stop();
  if (threadsafe_function_) {
    ReleaseThreadsafeFunction(napi_tsfn_abort);
  }
}

bool Win32Host::IsValid() const {
  return threadsafe_function_ != nullptr;
}

bool Win32Host::Start() {
  if (!IsValid()) {
    return false;
  }
  if (thread_) {
    return true;
  }

  stopping_ = false;
  start_succeeded_ = false;
  ready_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!ready_event_) {
    return false;
  }

  thread_ = CreateThread(nullptr, 0, ThreadEntry, this, 0, &thread_id_);
  if (!thread_) {
    CloseHandle(ready_event_);
    ready_event_ = nullptr;
    return false;
  }

  const DWORD wait_result = WaitForSingleObject(ready_event_, 5000);
  CloseHandle(ready_event_);
  ready_event_ = nullptr;

  if (wait_result != WAIT_OBJECT_0 || !start_succeeded_) {
    Stop();
    return false;
  }

  return true;
}

void Win32Host::Stop() {
  HANDLE thread = thread_;
  if (!thread) {
    return;
  }

  stopping_ = true;
  if (owner_window_) {
    PostMessageW(owner_window_, kStopMessage, 0, 0);
  } else if (thread_id_) {
    PostThreadMessageW(thread_id_, WM_QUIT, 0, 0);
  }

  WaitForSingleObject(thread, 5000);
  CloseHandle(thread);
  thread_ = nullptr;
  thread_id_ = 0;
}

bool Win32Host::UpdateTray(bool enabled,
                           const std::string& trigger_mode,
                           bool auto_start,
                           const std::string& indicator_action,
                           int icon_size,
                           int dot_size,
                           const std::string& icon_path,
                           const std::string& custom_shortcut) {
  if (!owner_window_ || stopping_) {
    return false;
  }

  auto request = std::make_unique<TrayStateRequest>();
  request->enabled = enabled;
  request->trigger_mode = trigger_mode;
  request->auto_start = auto_start;
  request->indicator_action = indicator_action;
  request->icon_size = std::clamp(icon_size, 12, 64);
  request->dot_size = std::clamp(dot_size, 6, 32);
  request->icon_path = icon_path;
  request->custom_shortcut = custom_shortcut;
  if (!PostMessageW(owner_window_, kUpdateTrayMessage, 0, reinterpret_cast<LPARAM>(request.get()))) {
    return false;
  }
  request.release();
  return true;
}

bool Win32Host::ShowIndicator(int x,
                              int y,
                              const std::string& style,
                              int size,
                              const std::string& icon_path,
                              bool hover_enabled,
                              unsigned int hover_delay_ms) {
  if (!owner_window_ || stopping_) {
    return false;
  }

  auto request = std::make_unique<IndicatorRequest>();
  request->x = x;
  request->y = y;
  request->style = style;
  request->size = std::clamp(size, 6, 64);
  request->icon_path = icon_path;
  request->hover_enabled = hover_enabled;
  request->hover_delay_ms = std::max(100U, hover_delay_ms);
  if (!PostMessageW(owner_window_, kShowIndicatorMessage, 0, reinterpret_cast<LPARAM>(request.get()))) {
    return false;
  }
  request.release();
  return true;
}

bool Win32Host::HideIndicator() {
  return owner_window_ && PostMessageW(owner_window_, kHideIndicatorMessage, 0, 0);
}

Win32Host::ShortcutResult Win32Host::RegisterShortcut(const std::string& shortcut) {
  ShortcutRequest request;
  request.shortcut = shortcut;
  if (!owner_window_ || stopping_) {
    request.result.error_code = ERROR_INVALID_WINDOW_HANDLE;
    return request.result;
  }
  SendMessageW(owner_window_,
               kRegisterShortcutMessage,
               0,
               reinterpret_cast<LPARAM>(&request));
  return request.result;
}

bool Win32Host::SetAutoStart(bool enabled,
                             const std::wstring& executable_path,
                             const std::wstring& arguments_text) {
  HKEY key = nullptr;
  const LSTATUS open_status = RegCreateKeyExW(HKEY_CURRENT_USER,
                                               kRunKeyPath,
                                               0,
                                               nullptr,
                                               REG_OPTION_NON_VOLATILE,
                                               KEY_SET_VALUE,
                                               nullptr,
                                               &key,
                                               nullptr);
  if (open_status != ERROR_SUCCESS) {
    return false;
  }

  LSTATUS result = ERROR_SUCCESS;
  if (enabled) {
    std::wstring command = QuoteExecutable(executable_path);
    if (!arguments_text.empty()) {
      command += L" ";
      command += arguments_text;
    }
    result = RegSetValueExW(key,
                            kRunValueName,
                            0,
                            REG_SZ,
                            reinterpret_cast<const BYTE*>(command.c_str()),
                            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
  } else {
    result = RegDeleteValueW(key, kRunValueName);
    if (result == ERROR_FILE_NOT_FOUND) {
      result = ERROR_SUCCESS;
    }
  }

  RegCloseKey(key);
  return result == ERROR_SUCCESS;
}

DWORD WINAPI Win32Host::ThreadEntry(void* parameter) {
  return static_cast<Win32Host*>(parameter)->ThreadMain();
}

DWORD Win32Host::ThreadMain() {
  using SetThreadDpiAwarenessContextFunction = DPI_AWARENESS_CONTEXT(WINAPI*)(DPI_AWARENESS_CONTEXT);
  auto* user32 = GetModuleHandleW(L"user32.dll");
  auto* set_thread_dpi = reinterpret_cast<SetThreadDpiAwarenessContextFunction>(
      GetProcAddress(user32, "SetThreadDpiAwarenessContext"));
  DPI_AWARENESS_CONTEXT previous_dpi_context = nullptr;
  if (set_thread_dpi) {
    previous_dpi_context = set_thread_dpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  }

  MSG message{};
  PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&Win32Host::ThreadEntry),
                     &instance_);
  if (!instance_) {
    instance_ = GetModuleHandleW(nullptr);
  }
  const bool initialized = RegisterWindowClasses() && CreateWindows() && AddTrayIcon();
  start_succeeded_ = initialized;
  if (ready_event_) {
    SetEvent(ready_event_);
  }

  if (initialized) {
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }

  RemoveTrayIcon();
  DestroyWindows();
  UnregisterClassW(kShortcutClassName, instance_);
  UnregisterClassW(kIndicatorClassName, instance_);
  UnregisterClassW(kOwnerClassName, instance_);

  if (set_thread_dpi && previous_dpi_context) {
    set_thread_dpi(previous_dpi_context);
  }

  ReleaseThreadsafeFunction(napi_tsfn_release);
  return initialized ? 0 : 1;
}

bool Win32Host::RegisterWindowClasses() {
  WNDCLASSEXW owner_class{};
  owner_class.cbSize = sizeof(owner_class);
  owner_class.hInstance = instance_;
  owner_class.lpfnWndProc = OwnerWindowProc;
  owner_class.lpszClassName = kOwnerClassName;
  owner_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);

  WNDCLASSEXW indicator_class{};
  indicator_class.cbSize = sizeof(indicator_class);
  indicator_class.hInstance = instance_;
  indicator_class.lpfnWndProc = IndicatorWindowProc;
  indicator_class.lpszClassName = kIndicatorClassName;
  indicator_class.hCursor = LoadCursorW(nullptr, IDC_HAND);

  WNDCLASSEXW shortcut_class{};
  shortcut_class.cbSize = sizeof(shortcut_class);
  shortcut_class.hInstance = instance_;
  shortcut_class.lpfnWndProc = ShortcutWindowProc;
  shortcut_class.lpszClassName = kShortcutClassName;
  shortcut_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  shortcut_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

  return RegisterClassExW(&owner_class) && RegisterClassExW(&indicator_class) &&
         RegisterClassExW(&shortcut_class);
}

bool Win32Host::CreateWindows() {
  owner_window_ = CreateWindowExW(WS_EX_TOOLWINDOW,
                                  kOwnerClassName,
                                  L"Selection Forward",
                                  WS_OVERLAPPED,
                                  0,
                                  0,
                                  0,
                                  0,
                                  nullptr,
                                  nullptr,
                                  instance_,
                                  this);
  if (!owner_window_) {
    return false;
  }

  indicator_window_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                                      kIndicatorClassName,
                                      L"",
                                      WS_POPUP,
                                      0,
                                      0,
                                      24,
                                      24,
                                      owner_window_,
                                      nullptr,
                                      instance_,
                                      this);
  return indicator_window_ != nullptr;
}

void Win32Host::DestroyWindows() {
  ClearShortcut();
  CloseShortcutCapture();
  if (indicator_window_) {
    DestroyWindow(indicator_window_);
    indicator_window_ = nullptr;
  }
  if (indicator_icon_) {
    DestroyIcon(indicator_icon_);
    indicator_icon_ = nullptr;
  }
  if (owner_window_) {
    DestroyWindow(owner_window_);
    owner_window_ = nullptr;
  }
}

bool Win32Host::AddTrayIcon() {
  tray_data_ = {};
  tray_data_.cbSize = sizeof(tray_data_);
  tray_data_.hWnd = owner_window_;
  tray_data_.uID = kTrayIconId;
  tray_data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  tray_data_.uCallbackMessage = kTrayCallbackMessage;
  tray_data_.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_SELECTION_FORWARD));
  if (!tray_data_.hIcon) {
    tray_data_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  }
  lstrcpynW(tray_data_.szTip, kTrayTooltip, ARRAYSIZE(tray_data_.szTip));

  taskbar_created_message_ = RegisterWindowMessageW(L"TaskbarCreated");
  tray_added_ = Shell_NotifyIconW(NIM_ADD, &tray_data_) == TRUE;
  return tray_added_;
}

void Win32Host::RemoveTrayIcon() {
  if (tray_added_) {
    Shell_NotifyIconW(NIM_DELETE, &tray_data_);
    tray_added_ = false;
  }
}

void Win32Host::ShowTrayMenu() {
  HMENU menu = CreatePopupMenu();
  HMENU trigger_menu = CreatePopupMenu();
  HMENU icon_size_menu = CreatePopupMenu();
  HMENU dot_size_menu = CreatePopupMenu();
  if (!menu || !trigger_menu || !icon_size_menu || !dot_size_menu) {
    if (dot_size_menu) DestroyMenu(dot_size_menu);
    if (icon_size_menu) DestroyMenu(icon_size_menu);
    if (trigger_menu) DestroyMenu(trigger_menu);
    if (menu) DestroyMenu(menu);
    return;
  }

  AppendMenuW(menu,
              MF_STRING | (enabled_ ? MF_CHECKED : MF_UNCHECKED),
              kCommandToggleEnabled,
              L"启用划词翻译");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

  AppendMenuW(trigger_menu, MF_STRING, kCommandImmediate, L"立即翻译");
  AppendMenuW(trigger_menu, MF_STRING, kCommandIcon, L"显示图标");
  AppendMenuW(trigger_menu, MF_STRING, kCommandDot, L"显示小圆点");
  AppendMenuW(trigger_menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(trigger_menu, MF_STRING, kCommandCtrl, L"按 Ctrl 触发");
  AppendMenuW(trigger_menu, MF_STRING, kCommandAlt, L"按 Alt 触发");
  AppendMenuW(trigger_menu, MF_STRING, kCommandShift, L"按 Shift 触发");
  AppendMenuW(trigger_menu, MF_STRING, kCommandCustom, L"自定义快捷键");

  const UINT checked_command = trigger_mode_ == "icon"      ? kCommandIcon
                               : trigger_mode_ == "dot"     ? kCommandDot
                               : trigger_mode_ == "ctrl"    ? kCommandCtrl
                               : trigger_mode_ == "alt"     ? kCommandAlt
                               : trigger_mode_ == "shift"   ? kCommandShift
                               : trigger_mode_ == "custom"  ? kCommandCustom
                                                              : kCommandImmediate;
  CheckMenuRadioItem(trigger_menu,
                     kCommandImmediate,
                     kCommandCustom,
                     checked_command,
                     MF_BYCOMMAND);

  AppendMenuW(menu,
              MF_POPUP,
              reinterpret_cast<UINT_PTR>(trigger_menu),
              L"触发方式");

  AppendMenuW(menu,
              MF_STRING | (indicator_action_ == "hover" ? MF_CHECKED : MF_UNCHECKED),
              kCommandToggleHover,
              L"悬浮时触发（点击始终可用）");

  AppendMenuW(icon_size_menu, MF_STRING, kCommandIconSize16, L"16 px");
  AppendMenuW(icon_size_menu, MF_STRING, kCommandIconSize20, L"20 px");
  AppendMenuW(icon_size_menu, MF_STRING, kCommandIconSize24, L"24 px");
  AppendMenuW(icon_size_menu, MF_STRING, kCommandIconSize32, L"32 px");
  AppendMenuW(icon_size_menu, MF_STRING, kCommandIconSize40, L"40 px");
  const UINT checked_icon_size = icon_size_ <= 16   ? kCommandIconSize16
                                 : icon_size_ <= 20 ? kCommandIconSize20
                                 : icon_size_ <= 24 ? kCommandIconSize24
                                 : icon_size_ <= 32 ? kCommandIconSize32
                                                    : kCommandIconSize40;
  CheckMenuRadioItem(icon_size_menu,
                     kCommandIconSize16,
                     kCommandIconSize40,
                     checked_icon_size,
                     MF_BYCOMMAND);
  AppendMenuW(menu,
              MF_POPUP,
              reinterpret_cast<UINT_PTR>(icon_size_menu),
              L"图标大小");

  AppendMenuW(dot_size_menu, MF_STRING, kCommandDotSize8, L"8 px");
  AppendMenuW(dot_size_menu, MF_STRING, kCommandDotSize12, L"12 px");
  AppendMenuW(dot_size_menu, MF_STRING, kCommandDotSize16, L"16 px");
  AppendMenuW(dot_size_menu, MF_STRING, kCommandDotSize20, L"20 px");
  AppendMenuW(dot_size_menu, MF_STRING, kCommandDotSize24, L"24 px");
  const UINT checked_dot_size = dot_size_ <= 8    ? kCommandDotSize8
                                : dot_size_ <= 12 ? kCommandDotSize12
                                : dot_size_ <= 16 ? kCommandDotSize16
                                : dot_size_ <= 20 ? kCommandDotSize20
                                                  : kCommandDotSize24;
  CheckMenuRadioItem(dot_size_menu,
                     kCommandDotSize8,
                     kCommandDotSize24,
                     checked_dot_size,
                     MF_BYCOMMAND);
  AppendMenuW(menu,
              MF_POPUP,
              reinterpret_cast<UINT_PTR>(dot_size_menu),
              L"圆点大小");
  AppendMenuW(menu, MF_STRING, kCommandChooseIcon, L"选择自定义图标…");
  AppendMenuW(menu,
              MF_STRING | (icon_path_.empty() ? MF_GRAYED : MF_ENABLED),
              kCommandDefaultIcon,
              L"恢复默认图标");

  const std::wstring shortcut_label =
      L"设置自定义快捷键…（" + Utf8ToWideLocal(custom_shortcut_) + L"）";
  AppendMenuW(menu, MF_STRING, kCommandSetShortcut, shortcut_label.c_str());
  AppendMenuW(menu,
              MF_STRING | (auto_start_ ? MF_CHECKED : MF_UNCHECKED),
              kCommandToggleAutoStart,
              L"开机启动");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kCommandExit, L"退出");

  POINT cursor{};
  GetCursorPos(&cursor);
  SetForegroundWindow(owner_window_);
  const UINT command = TrackPopupMenu(menu,
                                      TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                                      cursor.x,
                                      cursor.y,
                                      0,
                                      owner_window_,
                                      nullptr);
  DestroyMenu(menu);
  if (command) {
    HandleTrayCommand(command);
  }
  PostMessageW(owner_window_, WM_NULL, 0, 0);
}

void Win32Host::HandleTrayCommand(unsigned int command) {
  switch (command) {
    case kCommandToggleEnabled:
      SendEvent("toggle-enabled");
      break;
    case kCommandImmediate:
      SendEvent("set-trigger-mode", "immediate");
      break;
    case kCommandIcon:
      SendEvent("set-trigger-mode", "icon");
      break;
    case kCommandDot:
      SendEvent("set-trigger-mode", "dot");
      break;
    case kCommandCtrl:
      SendEvent("set-trigger-mode", "ctrl");
      break;
    case kCommandAlt:
      SendEvent("set-trigger-mode", "alt");
      break;
    case kCommandShift:
      SendEvent("set-trigger-mode", "shift");
      break;
    case kCommandCustom: {
      const ShortcutResult result = ApplyShortcut(custom_shortcut_);
      if (result.ok) {
        SendEvent("set-trigger-mode", "custom");
      } else {
        MessageBoxW(owner_window_,
                    L"该快捷键无效或已被其他应用占用。请先设置新的快捷键。",
                    L"Selection Forward",
                    MB_OK | MB_ICONWARNING);
      }
      break;
    }
    case kCommandToggleHover:
      SendEvent("toggle-hover");
      break;
    case kCommandIconSize16: SendEvent("set-icon-size", "16"); break;
    case kCommandIconSize20: SendEvent("set-icon-size", "20"); break;
    case kCommandIconSize24: SendEvent("set-icon-size", "24"); break;
    case kCommandIconSize32: SendEvent("set-icon-size", "32"); break;
    case kCommandIconSize40: SendEvent("set-icon-size", "40"); break;
    case kCommandDotSize8: SendEvent("set-dot-size", "8"); break;
    case kCommandDotSize12: SendEvent("set-dot-size", "12"); break;
    case kCommandDotSize16: SendEvent("set-dot-size", "16"); break;
    case kCommandDotSize20: SendEvent("set-dot-size", "20"); break;
    case kCommandDotSize24: SendEvent("set-dot-size", "24"); break;
    case kCommandChooseIcon:
      ChooseIndicatorIcon();
      break;
    case kCommandDefaultIcon:
      SendEvent("set-icon-path", "");
      break;
    case kCommandSetShortcut:
      ShowShortcutCapture();
      break;
    case kCommandToggleAutoStart:
      SendEvent("toggle-auto-start");
      break;
    case kCommandExit:
      SendEvent("exit");
      break;
  }
}

void Win32Host::ChooseIndicatorIcon() {
  wchar_t path[MAX_PATH]{};
  OPENFILENAMEW dialog{};
  dialog.lStructSize = sizeof(dialog);
  dialog.hwndOwner = owner_window_;
  dialog.lpstrFilter = L"Icon files (*.ico)\0*.ico\0All files (*.*)\0*.*\0";
  dialog.lpstrFile = path;
  dialog.nMaxFile = ARRAYSIZE(path);
  dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  dialog.lpstrDefExt = L"ico";
  if (GetOpenFileNameW(&dialog)) {
    SendEvent("set-icon-path", WideToUtf8(path));
  }
}

void Win32Host::ShowShortcutCapture() {
  if (shortcut_window_) {
    ShowWindow(shortcut_window_, SW_RESTORE);
    SetForegroundWindow(shortcut_window_);
    SetFocus(shortcut_window_);
    return;
  }

  captured_shortcut_.clear();
  shortcut_window_ = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW,
                                     kShortcutClassName,
                                     L"设置自定义快捷键",
                                     WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                     CW_USEDEFAULT,
                                     CW_USEDEFAULT,
                                     400,
                                     190,
                                     owner_window_,
                                     nullptr,
                                     instance_,
                                     this);
  if (!shortcut_window_) {
    return;
  }

  HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  HWND instructions = CreateWindowExW(0,
                                      L"STATIC",
                                      L"请按下包含修饰键的组合，例如 Ctrl+Alt+G：",
                                      WS_CHILD | WS_VISIBLE,
                                      20,
                                      18,
                                      340,
                                      24,
                                      shortcut_window_,
                                      nullptr,
                                      instance_,
                                      nullptr);
  shortcut_value_label_ = CreateWindowExW(WS_EX_CLIENTEDGE,
                                          L"STATIC",
                                          Utf8ToWideLocal(custom_shortcut_).c_str(),
                                          WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
                                          20,
                                          52,
                                          340,
                                          36,
                                          shortcut_window_,
                                          nullptr,
                                          instance_,
                                          nullptr);
  HWND save_button = CreateWindowExW(0,
                                     L"BUTTON",
                                     L"保存",
                                     WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                     194,
                                     108,
                                     80,
                                     28,
                                     shortcut_window_,
                                     reinterpret_cast<HMENU>(
                                         static_cast<INT_PTR>(kShortcutSaveButton)),
                                     instance_,
                                     nullptr);
  HWND cancel_button = CreateWindowExW(0,
                                       L"BUTTON",
                                       L"取消",
                                       WS_CHILD | WS_VISIBLE,
                                       282,
                                       108,
                                       80,
                                       28,
                                       shortcut_window_,
                                       reinterpret_cast<HMENU>(
                                           static_cast<INT_PTR>(kShortcutCancelButton)),
                                       instance_,
                                       nullptr);
  for (HWND control : {instructions, shortcut_value_label_, save_button, cancel_button}) {
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  }

  ShowWindow(shortcut_window_, SW_SHOW);
  SetForegroundWindow(shortcut_window_);
  SetFocus(shortcut_window_);
}

void Win32Host::CloseShortcutCapture() {
  if (shortcut_window_) {
    DestroyWindow(shortcut_window_);
    shortcut_window_ = nullptr;
    shortcut_value_label_ = nullptr;
  }
  captured_shortcut_.clear();
}

void Win32Host::ApplyIndicator(const IndicatorRequest& request) {
  if (!indicator_window_) {
    return;
  }

  POINT anchor{request.x, request.y};
  if (request.x == INT_MIN || request.y == INT_MIN) {
    GetCursorPos(&anchor);
  }

  indicator_style_ = request.style == "dot" ? "dot" : "icon";
  indicator_size_ = request.size;
  indicator_icon_path_ = request.icon_path;
  indicator_hover_enabled_ = request.hover_enabled;
  indicator_hover_delay_ms_ = request.hover_delay_ms;
  EndIndicatorHoverTracking();

  const int size = indicator_size_;
  const int offset = 8;
  int x = anchor.x + offset;
  int y = anchor.y + offset;

  HMONITOR monitor = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
  MONITORINFO monitor_info{};
  monitor_info.cbSize = sizeof(monitor_info);
  if (GetMonitorInfoW(monitor, &monitor_info)) {
    x = std::clamp(x,
                   static_cast<int>(monitor_info.rcWork.left),
                   static_cast<int>(monitor_info.rcWork.right) - size);
    y = std::clamp(y,
                   static_cast<int>(monitor_info.rcWork.top),
                   static_cast<int>(monitor_info.rcWork.bottom) - size);
  }

  HRGN region = indicator_style_ == "dot" ? CreateEllipticRgn(0, 0, size, size)
                                           : CreateRoundRectRgn(0, 0, size + 1, size + 1, 8, 8);
  if (region && !SetWindowRgn(indicator_window_, region, FALSE)) {
    DeleteObject(region);
  }

  if (indicator_icon_) {
    DestroyIcon(indicator_icon_);
    indicator_icon_ = nullptr;
  }
  if (indicator_style_ == "icon" && !indicator_icon_path_.empty()) {
    const std::wstring icon_path = Utf8ToWideLocal(indicator_icon_path_);
    indicator_icon_ = static_cast<HICON>(LoadImageW(nullptr,
                                                    icon_path.c_str(),
                                                    IMAGE_ICON,
                                                    size,
                                                    size,
                                                    LR_LOADFROMFILE));
  }
  if (indicator_style_ == "icon" && !indicator_icon_) {
    indicator_icon_ = static_cast<HICON>(LoadImageW(instance_,
                                                    MAKEINTRESOURCEW(IDI_SELECTION_FORWARD),
                                                    IMAGE_ICON,
                                                    size,
                                                    size,
                                                    LR_DEFAULTCOLOR));
  }

  SetWindowPos(indicator_window_,
               HWND_TOPMOST,
               x,
               y,
               size,
               size,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
  InvalidateRect(indicator_window_, nullptr, TRUE);
  UpdateWindow(indicator_window_);
}

void Win32Host::PaintIndicator(HWND window) {
  PAINTSTRUCT paint{};
  HDC device = BeginPaint(window, &paint);
  RECT bounds{};
  GetClientRect(window, &bounds);

  HBRUSH brush = CreateSolidBrush(kIndicatorColor);
  FillRect(device, &bounds, brush);
  DeleteObject(brush);

  if (indicator_style_ == "icon" && indicator_icon_) {
    DrawIconEx(device,
               0,
               0,
               indicator_icon_,
               indicator_size_,
               indicator_size_,
               0,
               nullptr,
               DI_NORMAL);
  } else if (indicator_style_ == "icon") {
    SetBkMode(device, TRANSPARENT);
    SetTextColor(device, RGB(255, 255, 255));
    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HGDIOBJ previous_font = SelectObject(device, font);
    DrawTextW(device, L"G", 1, &bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(device, previous_font);
  }

  EndPaint(window, &paint);
}

void Win32Host::BeginIndicatorHoverTracking() {
  if (indicator_tracking_ || !indicator_window_) {
    return;
  }

  TRACKMOUSEEVENT tracking{};
  tracking.cbSize = sizeof(tracking);
  tracking.dwFlags = TME_LEAVE;
  tracking.hwndTrack = indicator_window_;
  if (TrackMouseEvent(&tracking)) {
    indicator_tracking_ = true;
  }

  if (indicator_hover_enabled_) {
    SetTimer(indicator_window_, kHoverTimerId, indicator_hover_delay_ms_, nullptr);
  }
}

void Win32Host::EndIndicatorHoverTracking() {
  if (indicator_window_) {
    KillTimer(indicator_window_, kHoverTimerId);
  }
  indicator_tracking_ = false;
}

Win32Host::ShortcutResult Win32Host::ApplyShortcut(const std::string& shortcut) {
  ShortcutResult result;
  if (shortcut.empty()) {
    ClearShortcut();
    result.ok = true;
    return result;
  }

  const ParsedHotkey parsed = ParseHotkey(shortcut);
  if (!parsed.ok) {
    result.error_code = ERROR_INVALID_PARAMETER;
    return result;
  }
  result.normalized = parsed.normalized;

  if (hotkey_registered_ && parsed.modifiers == hotkey_modifiers_ &&
      parsed.virtual_key == hotkey_virtual_key_) {
    result.ok = true;
    return result;
  }

  if (!RegisterHotKey(owner_window_,
                      kHotkeyProbeId,
                      parsed.modifiers | MOD_NOREPEAT,
                      parsed.virtual_key)) {
    result.error_code = GetLastError();
    return result;
  }
  UnregisterHotKey(owner_window_, kHotkeyProbeId);

  ClearShortcut();
  if (!RegisterHotKey(owner_window_,
                      kHotkeyId,
                      parsed.modifiers | MOD_NOREPEAT,
                      parsed.virtual_key)) {
    result.error_code = GetLastError();
    return result;
  }

  hotkey_registered_ = true;
  hotkey_modifiers_ = parsed.modifiers;
  hotkey_virtual_key_ = parsed.virtual_key;
  custom_shortcut_ = parsed.normalized;
  result.ok = true;
  return result;
}

void Win32Host::ClearShortcut() {
  if (hotkey_registered_ && owner_window_) {
    UnregisterHotKey(owner_window_, kHotkeyId);
  }
  hotkey_registered_ = false;
  hotkey_modifiers_ = 0;
  hotkey_virtual_key_ = 0;
}

void Win32Host::SendEvent(const std::string& type, const std::string& value) {
  if (!threadsafe_function_ || stopping_) {
    return;
  }

  auto payload = std::make_unique<EventPayload>();
  payload->type = type;
  payload->value = value;
  const napi_status status =
      napi_call_threadsafe_function(threadsafe_function_, payload.get(), napi_tsfn_nonblocking);
  if (status == napi_ok) {
    payload.release();
  }
}

void Win32Host::ReleaseThreadsafeFunction(napi_threadsafe_function_release_mode mode) {
  if (threadsafe_function_) {
    napi_release_threadsafe_function(threadsafe_function_, mode);
    threadsafe_function_ = nullptr;
  }
}

void Win32Host::CallJs(napi_env env, napi_value callback, void*, void* data) {
  std::unique_ptr<EventPayload> payload(static_cast<EventPayload*>(data));
  if (!env || !callback || !payload) {
    return;
  }

  napi_value receiver = nullptr;
  napi_get_undefined(env, &receiver);
  napi_value arguments[2]{};
  napi_create_string_utf8(env, payload->type.c_str(), payload->type.size(), &arguments[0]);
  napi_create_string_utf8(env, payload->value.c_str(), payload->value.size(), &arguments[1]);
  napi_call_function(env, receiver, callback, 2, arguments, nullptr);
}

LRESULT CALLBACK Win32Host::OwnerWindowProc(HWND window,
                                             UINT message,
                                             WPARAM wparam,
                                             LPARAM lparam) {
  if (message == WM_NCCREATE) {
    AttachHost(window, lparam);
  }

  Win32Host* host = GetHost(window);
  if (!host) {
    return DefWindowProcW(window, message, wparam, lparam);
  }

  if (host->taskbar_created_message_ && message == host->taskbar_created_message_) {
    host->tray_added_ = false;
    host->AddTrayIcon();
    return 0;
  }

  switch (message) {
    case kTrayCallbackMessage:
      if (lparam == WM_RBUTTONUP || lparam == WM_CONTEXTMENU) {
        host->ShowTrayMenu();
      } else if (lparam == WM_LBUTTONUP || lparam == WM_LBUTTONDBLCLK) {
        host->SendEvent("open-settings");
      }
      return 0;
    case kShowIndicatorMessage: {
      std::unique_ptr<IndicatorRequest> request(reinterpret_cast<IndicatorRequest*>(lparam));
      host->ApplyIndicator(*request);
      return 0;
    }
    case kHideIndicatorMessage:
      host->EndIndicatorHoverTracking();
      ShowWindow(host->indicator_window_, SW_HIDE);
      return 0;
    case kUpdateTrayMessage: {
      std::unique_ptr<TrayStateRequest> request(reinterpret_cast<TrayStateRequest*>(lparam));
      host->enabled_ = request->enabled;
      host->trigger_mode_ = request->trigger_mode;
      host->auto_start_ = request->auto_start;
      host->indicator_action_ = request->indicator_action;
      host->icon_size_ = request->icon_size;
      host->dot_size_ = request->dot_size;
      host->icon_path_ = request->icon_path;
      host->custom_shortcut_ = request->custom_shortcut;
      if (host->trigger_mode_ == "custom") {
        const ShortcutResult result = host->ApplyShortcut(host->custom_shortcut_);
        if (!result.ok) {
          host->SendEvent("shortcut-conflict",
                          host->custom_shortcut_ + ":" + std::to_string(result.error_code));
        }
      } else {
        host->ClearShortcut();
      }
      return 0;
    }
    case kRegisterShortcutMessage: {
      auto* request = reinterpret_cast<ShortcutRequest*>(lparam);
      request->result = host->ApplyShortcut(request->shortcut);
      return 0;
    }
    case kStopMessage:
      host->RemoveTrayIcon();
      host->ClearShortcut();
      host->CloseShortcutCapture();
      if (host->indicator_window_) {
        DestroyWindow(host->indicator_window_);
        host->indicator_window_ = nullptr;
      }
      DestroyWindow(window);
      return 0;
    case WM_HOTKEY:
      if (wparam == kHotkeyId) {
        host->SendEvent("shortcut", host->custom_shortcut_);
      }
      return 0;
    case WM_DESTROY:
      host->owner_window_ = nullptr;
      PostQuitMessage(0);
      return 0;
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32Host::ShortcutWindowProc(HWND window,
                                                UINT message,
                                                WPARAM wparam,
                                                LPARAM lparam) {
  if (message == WM_NCCREATE) {
    AttachHost(window, lparam);
  }

  Win32Host* host = GetHost(window);
  if (!host) {
    return DefWindowProcW(window, message, wparam, lparam);
  }

  switch (message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
      if (wparam == VK_ESCAPE) {
        host->CloseShortcutCapture();
        return 0;
      }
      const std::string captured = CaptureHotkey(static_cast<UINT>(wparam));
      if (!captured.empty()) {
        host->captured_shortcut_ = captured;
        const std::wstring label = Utf8ToWideLocal(captured);
        SetWindowTextW(host->shortcut_value_label_, label.c_str());
      }
      return 0;
    }
    case WM_COMMAND:
      if (LOWORD(wparam) == kShortcutSaveButton) {
        const std::string shortcut = host->captured_shortcut_.empty()
                                         ? host->custom_shortcut_
                                         : host->captured_shortcut_;
        const ShortcutResult result = host->ApplyShortcut(shortcut);
        if (!result.ok) {
          const std::wstring message_text =
              L"快捷键无效或已被占用。Windows 错误码：" +
              std::to_wstring(result.error_code);
          MessageBoxW(window,
                      message_text.c_str(),
                      L"无法注册快捷键",
                      MB_OK | MB_ICONWARNING);
          SetFocus(window);
          return 0;
        }
        host->SendEvent("set-custom-shortcut", result.normalized);
        host->SendEvent("set-trigger-mode", "custom");
        host->CloseShortcutCapture();
        return 0;
      }
      if (LOWORD(wparam) == kShortcutCancelButton) {
        host->CloseShortcutCapture();
        return 0;
      }
      break;
    case WM_CLOSE:
      host->CloseShortcutCapture();
      return 0;
    case WM_DESTROY:
      host->shortcut_window_ = nullptr;
      host->shortcut_value_label_ = nullptr;
      host->captured_shortcut_.clear();
      return 0;
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32Host::IndicatorWindowProc(HWND window,
                                                 UINT message,
                                                 WPARAM wparam,
                                                 LPARAM lparam) {
  if (message == WM_NCCREATE) {
    AttachHost(window, lparam);
  }

  Win32Host* host = GetHost(window);
  if (!host) {
    return DefWindowProcW(window, message, wparam, lparam);
  }

  switch (message) {
    case WM_PAINT:
      host->PaintIndicator(window);
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_MOUSEMOVE:
      host->BeginIndicatorHoverTracking();
      return 0;
    case WM_MOUSELEAVE:
      host->EndIndicatorHoverTracking();
      return 0;
    case WM_TIMER:
      if (wparam == kHoverTimerId && host->indicator_hover_enabled_) {
        host->EndIndicatorHoverTracking();
        ShowWindow(window, SW_HIDE);
        host->SendEvent("indicator-hover");
      }
      return 0;
    case WM_LBUTTONUP:
      host->EndIndicatorHoverTracking();
      ShowWindow(window, SW_HIDE);
      host->SendEvent("indicator-click");
      return 0;
    case WM_DESTROY:
      host->indicator_window_ = nullptr;
      return 0;
  }

  return DefWindowProcW(window, message, wparam, lparam);
}
