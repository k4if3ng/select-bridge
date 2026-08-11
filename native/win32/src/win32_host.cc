#include "win32_host.h"

#include <shellapi.h>

#include <algorithm>
#include <memory>
#include <utility>

namespace {

constexpr wchar_t kOwnerClassName[] = L"SelectionForward.NativeHost";
constexpr wchar_t kIndicatorClassName[] = L"SelectionForward.Indicator";
constexpr wchar_t kTrayTooltip[] = L"Selection Forward";
constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"SelectionForward";

constexpr UINT kTrayIconId = 1;
constexpr UINT kTrayCallbackMessage = WM_APP + 1;
constexpr UINT kShowIndicatorMessage = WM_APP + 2;
constexpr UINT kHideIndicatorMessage = WM_APP + 3;
constexpr UINT kUpdateTrayMessage = WM_APP + 4;
constexpr UINT kStopMessage = WM_APP + 5;
constexpr UINT_PTR kHoverTimerId = 1;

constexpr UINT kCommandToggleEnabled = 1001;
constexpr UINT kCommandImmediate = 1100;
constexpr UINT kCommandIcon = 1101;
constexpr UINT kCommandDot = 1102;
constexpr UINT kCommandCtrl = 1103;
constexpr UINT kCommandAlt = 1104;
constexpr UINT kCommandShift = 1105;
constexpr UINT kCommandToggleAutoStart = 1200;
constexpr UINT kCommandSettings = 1300;
constexpr UINT kCommandExit = 1400;

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

}  // namespace

struct Win32Host::IndicatorRequest {
  int x;
  int y;
  std::string style;
  bool hover_enabled;
  unsigned int hover_delay_ms;
};

struct Win32Host::TrayStateRequest {
  bool enabled;
  bool auto_start;
  std::string trigger_mode;
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

bool Win32Host::UpdateTray(bool enabled, const std::string& trigger_mode, bool auto_start) {
  if (!owner_window_ || stopping_) {
    return false;
  }

  auto request = std::make_unique<TrayStateRequest>();
  request->enabled = enabled;
  request->trigger_mode = trigger_mode;
  request->auto_start = auto_start;
  if (!PostMessageW(owner_window_, kUpdateTrayMessage, 0, reinterpret_cast<LPARAM>(request.get()))) {
    return false;
  }
  request.release();
  return true;
}

bool Win32Host::ShowIndicator(int x,
                              int y,
                              const std::string& style,
                              bool hover_enabled,
                              unsigned int hover_delay_ms) {
  if (!owner_window_ || stopping_) {
    return false;
  }

  auto request = std::make_unique<IndicatorRequest>();
  request->x = x;
  request->y = y;
  request->style = style;
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

  instance_ = GetModuleHandleW(nullptr);
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

  return RegisterClassExW(&owner_class) && RegisterClassExW(&indicator_class);
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
  if (indicator_window_) {
    DestroyWindow(indicator_window_);
    indicator_window_ = nullptr;
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
  tray_data_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
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
  if (!menu || !trigger_menu) {
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

  const UINT checked_command = trigger_mode_ == "icon"      ? kCommandIcon
                               : trigger_mode_ == "dot"     ? kCommandDot
                               : trigger_mode_ == "ctrl"    ? kCommandCtrl
                               : trigger_mode_ == "alt"     ? kCommandAlt
                               : trigger_mode_ == "shift"   ? kCommandShift
                                                              : kCommandImmediate;
  CheckMenuRadioItem(trigger_menu,
                     kCommandImmediate,
                     kCommandShift,
                     checked_command,
                     MF_BYCOMMAND);

  AppendMenuW(menu,
              MF_POPUP,
              reinterpret_cast<UINT_PTR>(trigger_menu),
              L"触发方式");
  AppendMenuW(menu,
              MF_STRING | (auto_start_ ? MF_CHECKED : MF_UNCHECKED),
              kCommandToggleAutoStart,
              L"开机启动");
  AppendMenuW(menu, MF_STRING, kCommandSettings, L"设置…");
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
    case kCommandToggleAutoStart:
      SendEvent("toggle-auto-start");
      break;
    case kCommandSettings:
      SendEvent("open-settings");
      break;
    case kCommandExit:
      SendEvent("exit");
      break;
  }
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
  indicator_hover_enabled_ = request.hover_enabled;
  indicator_hover_delay_ms_ = request.hover_delay_ms;
  EndIndicatorHoverTracking();

  const int size = indicator_style_ == "dot" ? 12 : 24;
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

  if (indicator_style_ == "icon") {
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
      return 0;
    }
    case kStopMessage:
      host->RemoveTrayIcon();
      if (host->indicator_window_) {
        DestroyWindow(host->indicator_window_);
        host->indicator_window_ = nullptr;
      }
      DestroyWindow(window);
      return 0;
    case WM_DESTROY:
      host->owner_window_ = nullptr;
      PostQuitMessage(0);
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
