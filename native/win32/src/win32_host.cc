#include "win32_host.h"
#include "../resource.h"

#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

namespace {

constexpr wchar_t kOwnerClassName[] = L"SelectBridge.NativeHost";
constexpr wchar_t kIndicatorClassName[] = L"SelectBridge.Indicator";
constexpr wchar_t kShortcutClassName[] = L"SelectBridge.ShortcutCapture";
constexpr wchar_t kTargetUrlClassName[] = L"SelectBridge.TargetUrl";
constexpr wchar_t kTrayTooltip[] = L"SelectBridge";
constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"SelectBridge";

constexpr UINT kTrayIconId = 1;
constexpr UINT kTrayCallbackMessage = WM_APP + 1;
constexpr UINT kShowIndicatorMessage = WM_APP + 2;
constexpr UINT kHideIndicatorMessage = WM_APP + 3;
constexpr UINT kUpdateTrayMessage = WM_APP + 4;
constexpr UINT kStopMessage = WM_APP + 5;
constexpr UINT kRegisterShortcutMessage = WM_APP + 6;
constexpr UINT kTargetUrlSaveResultMessage = WM_APP + 7;
constexpr UINT kShowErrorMessage = WM_APP + 8;
constexpr UINT kShowInfoMessage = WM_APP + 9;
constexpr UINT kConfirmMessage = WM_APP + 10;
constexpr UINT kEditSetCueBannerMessage = 0x1501;
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
constexpr UINT kCommandIndicatorClick = 1201;
constexpr UINT kCommandIndicatorHover = 1202;
constexpr UINT kCommandSetShortcut = 1300;
constexpr UINT kCommandExit = 1400;
constexpr UINT kCommandIconSize24 = 1500;
constexpr UINT kCommandIconSize28 = 1501;
constexpr UINT kCommandIconSize32 = 1502;
constexpr UINT kCommandIconSize36 = 1503;
constexpr UINT kCommandIconSize40 = 1504;
constexpr UINT kCommandDotSize12 = 1510;
constexpr UINT kCommandDotSize16 = 1511;
constexpr UINT kCommandDotSize20 = 1512;
constexpr UINT kCommandDotSize24 = 1513;
constexpr UINT kCommandDotSize28 = 1514;
constexpr UINT kCommandTargetGoldendict = 1520;
constexpr UINT kCommandTargetCustom = 1521;
constexpr UINT kCommandSetTargetUrl = 1522;
constexpr UINT kCommandOpenConfigFile = 1530;
constexpr UINT kCommandOpenConfigDirectory = 1531;
constexpr UINT kCommandReloadConfig = 1532;
constexpr UINT kCommandCheckForUpdates = 1533;
constexpr UINT kCommandLanguageEnglish = 1540;
constexpr UINT kCommandLanguageSimplifiedChinese = 1541;
constexpr UINT kShortcutInstructions = 1600;
constexpr UINT kShortcutFieldLabel = 1601;
constexpr UINT kShortcutValueEdit = 1602;
constexpr UINT kShortcutStatusLabel = 1603;
constexpr UINT kShortcutRemoveButton = 1604;
constexpr UINT kShortcutCancelButton = 1605;
constexpr UINT kShortcutSaveButton = 1606;
constexpr UINT kShortcutValueFrame = 1607;
constexpr UINT kTargetUrlInstructions = 1700;
constexpr UINT kTargetUrlFieldLabel = 1701;
constexpr UINT kTargetUrlEdit = 1702;
constexpr UINT kTargetUrlStatusLabel = 1703;
constexpr UINT kTargetUrlCopyButton = 1704;
constexpr UINT kTargetUrlCancelButton = 1705;
constexpr UINT kTargetUrlSaveButton = 1706;
constexpr UINT kTargetUrlEditFrame = 1707;

constexpr COLORREF kIndicatorColor = RGB(31, 111, 235);
constexpr COLORREF kIndicatorBackgroundColor = RGB(255, 255, 255);
constexpr COLORREF kIndicatorBorderColor = RGB(156, 169, 186);

bool IsInsideRoundedRectangle(double x,
                              double y,
                              double left,
                              double top,
                              double right,
                              double bottom,
                              double radius) {
  if (x < left || x >= right || y < top || y >= bottom) {
    return false;
  }
  const double closest_x = std::clamp(x, left + radius, right - radius);
  const double closest_y = std::clamp(y, top + radius, bottom - radius);
  const double dx = x - closest_x;
  const double dy = y - closest_y;
  return dx * dx + dy * dy <= radius * radius;
}

double RoundedRectangleCoverage(int pixel_x,
                                int pixel_y,
                                double left,
                                double top,
                                double right,
                                double bottom,
                                double radius) {
  constexpr int kSamplesPerAxis = 4;
  int covered = 0;
  for (int sample_y = 0; sample_y < kSamplesPerAxis; ++sample_y) {
    for (int sample_x = 0; sample_x < kSamplesPerAxis; ++sample_x) {
      const double x = pixel_x + (sample_x + 0.5) / kSamplesPerAxis;
      const double y = pixel_y + (sample_y + 0.5) / kSamplesPerAxis;
      if (IsInsideRoundedRectangle(x, y, left, top, right, bottom, radius)) {
        ++covered;
      }
    }
  }
  return static_cast<double>(covered) / (kSamplesPerAxis * kSamplesPerAxis);
}

double CircleCoverage(int pixel_x, int pixel_y, double center, double radius) {
  constexpr int kSamplesPerAxis = 4;
  int covered = 0;
  for (int sample_y = 0; sample_y < kSamplesPerAxis; ++sample_y) {
    for (int sample_x = 0; sample_x < kSamplesPerAxis; ++sample_x) {
      const double x = pixel_x + (sample_x + 0.5) / kSamplesPerAxis - center;
      const double y = pixel_y + (sample_y + 0.5) / kSamplesPerAxis - center;
      if (x * x + y * y <= radius * radius) {
        ++covered;
      }
    }
  }
  return static_cast<double>(covered) / (kSamplesPerAxis * kSamplesPerAxis);
}

struct BgraPixel {
  std::uint8_t blue;
  std::uint8_t green;
  std::uint8_t red;
  std::uint8_t alpha;
};

void SetPremultipliedPixel(BgraPixel* pixel,
                           COLORREF color,
                           double color_coverage,
                           double alpha_coverage) {
  pixel->red = static_cast<std::uint8_t>(
      std::lround(GetRValue(color) * std::clamp(color_coverage, 0.0, 1.0)));
  pixel->green = static_cast<std::uint8_t>(
      std::lround(GetGValue(color) * std::clamp(color_coverage, 0.0, 1.0)));
  pixel->blue = static_cast<std::uint8_t>(
      std::lround(GetBValue(color) * std::clamp(color_coverage, 0.0, 1.0)));
  pixel->alpha = static_cast<std::uint8_t>(
      std::lround(255.0 * std::clamp(alpha_coverage, 0.0, 1.0)));
}

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

  const auto is_down = [](int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; };
  const bool control_down =
      is_down(VK_CONTROL) || is_down(VK_LCONTROL) || is_down(VK_RCONTROL);
  const bool alt_down = is_down(VK_MENU) || is_down(VK_LMENU) || is_down(VK_RMENU);
  const bool shift_down =
      is_down(VK_SHIFT) || is_down(VK_LSHIFT) || is_down(VK_RSHIFT);
  const bool win_down = is_down(VK_LWIN) || is_down(VK_RWIN);

  std::string shortcut;
  if (control_down) shortcut += "Ctrl+";
  if (alt_down) shortcut += "Alt+";
  if (shift_down) shortcut += "Shift+";
  if (win_down) shortcut += "Win+";
  if (shortcut.empty()) {
    return {};
  }

  const std::string key_name = VirtualKeyName(virtual_key);
  return key_name.empty() ? std::string{} : shortcut + key_name;
}

bool IsModifierVirtualKey(UINT virtual_key) {
  return virtual_key == VK_CONTROL || virtual_key == VK_LCONTROL ||
         virtual_key == VK_RCONTROL || virtual_key == VK_MENU ||
         virtual_key == VK_LMENU || virtual_key == VK_RMENU ||
         virtual_key == VK_SHIFT || virtual_key == VK_LSHIFT ||
         virtual_key == VK_RSHIFT || virtual_key == VK_LWIN ||
         virtual_key == VK_RWIN;
}

bool IsVirtualKeyDown(int virtual_key) {
  return (GetAsyncKeyState(virtual_key) & 0x8000) != 0;
}

bool HasShortcutModifierDown() {
  return IsVirtualKeyDown(VK_CONTROL) || IsVirtualKeyDown(VK_LCONTROL) ||
         IsVirtualKeyDown(VK_RCONTROL) || IsVirtualKeyDown(VK_MENU) ||
         IsVirtualKeyDown(VK_LMENU) || IsVirtualKeyDown(VK_RMENU) ||
         IsVirtualKeyDown(VK_SHIFT) || IsVirtualKeyDown(VK_LSHIFT) ||
         IsVirtualKeyDown(VK_RSHIFT) || IsVirtualKeyDown(VK_LWIN) ||
         IsVirtualKeyDown(VK_RWIN);
}

std::string CaptureModifierPreview() {
  std::string preview;
  if (IsVirtualKeyDown(VK_CONTROL) || IsVirtualKeyDown(VK_LCONTROL) ||
      IsVirtualKeyDown(VK_RCONTROL)) {
    preview += "Ctrl+";
  }
  if (IsVirtualKeyDown(VK_MENU) || IsVirtualKeyDown(VK_LMENU) ||
      IsVirtualKeyDown(VK_RMENU)) {
    preview += "Alt+";
  }
  if (IsVirtualKeyDown(VK_SHIFT) || IsVirtualKeyDown(VK_LSHIFT) ||
      IsVirtualKeyDown(VK_RSHIFT)) {
    preview += "Shift+";
  }
  if (IsVirtualKeyDown(VK_LWIN) || IsVirtualKeyDown(VK_RWIN)) {
    preview += "Win+";
  }
  if (!preview.empty()) {
    preview += "...";
  }
  return preview;
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

std::string WideToUtf8Local(const std::wstring& value) {
  if (value.empty()) return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                                       nullptr, 0, nullptr, nullptr);
  std::string result(size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                      result.data(), size, nullptr, nullptr);
  return result;
}

bool IsValidTargetUrl(const std::wstring& value) {
  if (value.empty() || value.size() > 2048) return false;
  size_t placeholder_count = 0;
  size_t position = 0;
  while ((position = value.find(L"{text}", position)) != std::wstring::npos) {
    ++placeholder_count;
    position += 6;
  }
  if (placeholder_count != 1 ||
      !((value[0] >= L'A' && value[0] <= L'Z') || (value[0] >= L'a' && value[0] <= L'z'))) {
    return false;
  }
  size_t scheme_end = 1;
  while (scheme_end < value.size() && value[scheme_end] != L':') {
    const wchar_t character = value[scheme_end];
    const bool ascii_alphanumeric =
        (character >= L'A' && character <= L'Z') ||
        (character >= L'a' && character <= L'z') ||
        (character >= L'0' && character <= L'9');
    if (!ascii_alphanumeric && character != L'+' && character != L'.' && character != L'-') {
      return false;
    }
    ++scheme_end;
  }
  if (scheme_end >= value.size() || value[scheme_end] != L':') return false;
  for (wchar_t character : value) {
    if (iswspace(character) || character < 0x20 || character == 0x7f) return false;
  }
  return true;
}

UINT GetWindowDpiCompat(HWND window) {
  using GetDpiForWindowFunction = UINT(WINAPI*)(HWND);
  auto* function = reinterpret_cast<GetDpiForWindowFunction>(
      GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
  return function ? function(window) : 96U;
}

int ScaleForDpi(int value, UINT dpi) {
  return MulDiv(value, static_cast<int>(dpi), 96);
}

int MeasureFontHeight(HWND window, HFONT font, int fallback) {
  HDC context = GetDC(window);
  if (!context) return fallback;
  HGDIOBJ previous = font ? SelectObject(context, font) : nullptr;
  TEXTMETRICW metrics{};
  const bool measured = GetTextMetricsW(context, &metrics) == TRUE;
  if (previous) SelectObject(context, previous);
  ReleaseDC(window, context);
  return measured ? metrics.tmHeight : fallback;
}

LANGID ResourceLanguageId(const std::string& ui_language) {
  return ui_language == "zh-CN"
             ? MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED)
             : MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
}

std::wstring LoadStringResource(HINSTANCE instance, UINT resource_id, LANGID language_id) {
  const UINT block_id = resource_id / 16 + 1;
  HRSRC resource = FindResourceExW(
      instance, RT_STRING, MAKEINTRESOURCEW(block_id), language_id);
  if (!resource) return {};
  HGLOBAL loaded = LoadResource(instance, resource);
  if (!loaded) return {};
  const auto* cursor = static_cast<const wchar_t*>(LockResource(loaded));
  const DWORD byte_size = SizeofResource(instance, resource);
  if (!cursor || byte_size < sizeof(wchar_t)) return {};
  const auto* end = cursor + byte_size / sizeof(wchar_t);
  const UINT index = resource_id % 16;
  for (UINT current = 0; current < 16 && cursor < end; ++current) {
    const UINT length = static_cast<UINT>(*cursor++);
    if (length > static_cast<UINT>(end - cursor)) return {};
    if (current == index) {
      return std::wstring(cursor, cursor + length);
    }
    cursor += length;
  }
  return {};
}

std::wstring LoadLocalizedString(HINSTANCE instance,
                                 UINT resource_id,
                                 const std::string& ui_language) {
  std::wstring value =
      LoadStringResource(instance, resource_id, ResourceLanguageId(ui_language));
  if (!value.empty() || ui_language == "en-US") return value;
  return LoadStringResource(
      instance, resource_id, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
}

std::wstring ReplacePlaceholder(std::wstring text,
                                const std::wstring& placeholder,
                                const std::wstring& value) {
  const size_t position = text.find(placeholder);
  if (position != std::wstring::npos) {
    text.replace(position, placeholder.size(), value);
  }
  return text;
}

void ResizeAndCenterWindow(HWND window,
                           int client_width,
                           int client_height,
                           DWORD style,
                           DWORD extended_style,
                           UINT dpi) {
  RECT bounds{0, 0, ScaleForDpi(client_width, dpi), ScaleForDpi(client_height, dpi)};
  using AdjustWindowRectExForDpiFunction = BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
  auto* adjust_for_dpi = reinterpret_cast<AdjustWindowRectExForDpiFunction>(
      GetProcAddress(GetModuleHandleW(L"user32.dll"), "AdjustWindowRectExForDpi"));
  if (adjust_for_dpi) {
    adjust_for_dpi(&bounds, style, FALSE, extended_style, dpi);
  } else {
    AdjustWindowRectEx(&bounds, style, FALSE, extended_style);
  }

  const int width = bounds.right - bounds.left;
  const int height = bounds.bottom - bounds.top;
  POINT cursor{};
  GetCursorPos(&cursor);
  HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
  MONITORINFO monitor_info{};
  monitor_info.cbSize = sizeof(monitor_info);
  int x = CW_USEDEFAULT;
  int y = CW_USEDEFAULT;
  UINT position_flags = SWP_NOACTIVATE | SWP_NOZORDER;
  if (GetMonitorInfoW(monitor, &monitor_info)) {
    x = monitor_info.rcWork.left + (monitor_info.rcWork.right - monitor_info.rcWork.left - width) / 2;
    y = monitor_info.rcWork.top + (monitor_info.rcWork.bottom - monitor_info.rcWork.top - height) / 2;
  } else {
    position_flags |= SWP_NOMOVE;
  }
  SetWindowPos(window, nullptr, x, y, width, height, position_flags);
}

}  // namespace

struct Win32Host::IndicatorRequest {
  int x;
  int y;
  std::string style;
  int size;
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
  std::string custom_shortcut;
  std::string target_mode;
  std::string custom_target_url;
  std::string target_override_source;
  std::string ui_language;
};

struct Win32Host::ShortcutRequest {
  std::string shortcut;
  ShortcutResult result;
};

struct Win32Host::EventPayload {
  std::string type;
  std::string value;
};

struct Win32Host::TargetSaveResult {
  bool ok;
  std::string message;
};

struct Win32Host::ErrorRequest {
  std::string title;
  std::string message;
};

struct Win32Host::DialogRequest {
  std::string title;
  std::string message;
  bool confirmed = false;
};

Win32Host::Win32Host(napi_env env, napi_value callback) : env_(env) {
  napi_value resource_name = nullptr;
  if (napi_create_string_utf8(env_, "select-bridge-win32", NAPI_AUTO_LENGTH, &resource_name) !=
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

  // The UI thread owns every HWND and the thread-safe function release. Never
  // destroy the host while it may still be executing on that thread.
  const DWORD wait_result = WaitForSingleObject(thread, 5000);
  if (wait_result == WAIT_TIMEOUT) {
    // A modal shortcut window or a delayed Win32 callback can outlive the
    // normal stop message. Keep waiting rather than risking use-after-free.
    WaitForSingleObject(thread, INFINITE);
  }
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
                           const std::string& custom_shortcut,
                           const std::string& target_mode,
                           const std::string& custom_target_url,
                           const std::string& target_override_source,
                           const std::string& ui_language) {
  if (!owner_window_ || stopping_) {
    return false;
  }

  auto request = std::make_unique<TrayStateRequest>();
  request->enabled = enabled;
  request->trigger_mode = trigger_mode;
  request->auto_start = auto_start;
  request->indicator_action = indicator_action;
  request->icon_size = std::clamp(icon_size, 24, 40);
  request->dot_size = std::clamp(dot_size, 12, 28);
  request->custom_shortcut = custom_shortcut;
  request->target_mode = target_mode;
  request->custom_target_url = custom_target_url;
  request->target_override_source = target_override_source;
  request->ui_language = ui_language == "zh-CN" ? "zh-CN" : "en-US";
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
                              bool hover_enabled,
                              unsigned int hover_delay_ms) {
  if (!owner_window_ || stopping_) {
    return false;
  }

  auto request = std::make_unique<IndicatorRequest>();
  request->x = x;
  request->y = y;
  request->style = style;
  request->size = std::clamp(size, 12, 64);
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

bool Win32Host::CompleteTargetUrlSave(bool ok, const std::string& message) {
  if (!owner_window_ || stopping_) return false;
  auto request = std::make_unique<TargetSaveResult>();
  request->ok = ok;
  request->message = message;
  if (!PostMessageW(owner_window_, kTargetUrlSaveResultMessage, 0,
                    reinterpret_cast<LPARAM>(request.get()))) {
    return false;
  }
  request.release();
  return true;
}

bool Win32Host::ShowError(const std::string& title, const std::string& message) {
  if (!owner_window_ || stopping_) return false;
  auto request = std::make_unique<ErrorRequest>();
  request->title = title;
  request->message = message;
  if (!PostMessageW(owner_window_, kShowErrorMessage, 0,
                    reinterpret_cast<LPARAM>(request.get()))) {
    return false;
  }
  request.release();
  return true;
}

bool Win32Host::ShowInfo(const std::string& title, const std::string& message) {
  if (!owner_window_ || stopping_) return false;
  DialogRequest request;
  request.title = title;
  request.message = message;
  SendMessageW(owner_window_, kShowInfoMessage, 0, reinterpret_cast<LPARAM>(&request));
  return true;
}

bool Win32Host::Confirm(const std::string& title, const std::string& message) {
  if (!owner_window_ || stopping_) return false;
  DialogRequest request;
  request.title = title;
  request.message = message;
  SendMessageW(owner_window_, kConfirmMessage, 0, reinterpret_cast<LPARAM>(&request));
  return request.confirmed;
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
                                               KEY_QUERY_VALUE | KEY_SET_VALUE,
                                               nullptr,
                                               &key,
                                               nullptr);
  if (open_status != ERROR_SUCCESS) {
    return false;
  }

  std::wstring command = QuoteExecutable(executable_path);
  if (!arguments_text.empty()) {
    command += L" ";
    command += arguments_text;
  }

  LSTATUS result = ERROR_SUCCESS;
  if (enabled) {
    result = RegSetValueExW(key,
                            kRunValueName,
                            0,
                            REG_SZ,
                            reinterpret_cast<const BYTE*>(command.c_str()),
                            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
  } else {
    DWORD value_type = 0;
    DWORD value_size = 0;
    result = RegQueryValueExW(
        key, kRunValueName, nullptr, &value_type, nullptr, &value_size);
    if (result == ERROR_FILE_NOT_FOUND) {
      result = ERROR_SUCCESS;
    } else if (result == ERROR_SUCCESS && value_type == REG_SZ) {
      std::vector<wchar_t> current_value(value_size / sizeof(wchar_t) + 1, L'\0');
      result = RegQueryValueExW(key,
                                kRunValueName,
                                nullptr,
                                &value_type,
                                reinterpret_cast<BYTE*>(current_value.data()),
                                &value_size);
      if (result == ERROR_SUCCESS &&
          _wcsicmp(current_value.data(), command.c_str()) == 0) {
        result = RegDeleteValueW(key, kRunValueName);
      }
    } else if (result == ERROR_SUCCESS) {
      // A value owned by another package or with an unexpected type is not ours
      // to remove.
      result = ERROR_SUCCESS;
    }
  }

  RegCloseKey(key);
  return result == ERROR_SUCCESS;
}

bool Win32Host::OpenExternalUrl(const std::wstring& url) {
  if (url.empty()) {
    return false;
  }
  const HINSTANCE result = ShellExecuteW(
      nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  return reinterpret_cast<INT_PTR>(result) > 32;
}

bool Win32Host::OpenPath(const std::wstring& path) {
  if (path.empty()) return false;
  const HINSTANCE result = ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  return reinterpret_cast<INT_PTR>(result) > 32;
}

std::string Win32Host::GetSystemUiLanguage() {
  ULONG language_count = 0;
  ULONG buffer_length = 0;
  if (GetUserPreferredUILanguages(
          MUI_LANGUAGE_NAME, &language_count, nullptr, &buffer_length) &&
      buffer_length > 1) {
    std::wstring buffer(buffer_length, L'\0');
    if (GetUserPreferredUILanguages(
            MUI_LANGUAGE_NAME, &language_count, buffer.data(), &buffer_length) &&
        !buffer.empty() && buffer.front() != L'\0') {
      return WideToUtf8Local(std::wstring(buffer.data()));
    }
  }

  wchar_t locale_name[LOCALE_NAME_MAX_LENGTH]{};
  if (GetUserDefaultLocaleName(locale_name, ARRAYSIZE(locale_name)) > 0) {
    return WideToUtf8Local(locale_name);
  }
  return "en-US";
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
      const bool shortcut_key_message =
          shortcut_window_ &&
          (message.message == WM_KEYDOWN || message.message == WM_SYSKEYDOWN) &&
          (message.hwnd == shortcut_window_ || IsChild(shortcut_window_, message.hwnd));
      if (shortcut_key_message) {
        ShortcutWindowProc(shortcut_window_,
                           message.message,
                           message.wParam,
                           message.lParam);
        continue;
      }
      const bool target_key_message =
          target_url_window_ &&
          (message.message == WM_KEYDOWN || message.message == WM_SYSKEYDOWN) &&
          (message.hwnd == target_url_window_ || IsChild(target_url_window_, message.hwnd)) &&
          (message.wParam == VK_ESCAPE || message.wParam == VK_RETURN || message.wParam == VK_TAB ||
           (message.wParam == VK_F4 && IsVirtualKeyDown(VK_MENU)));
      if (target_key_message) {
        TargetUrlWindowProc(target_url_window_, message.message, message.wParam, message.lParam);
        continue;
      }
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }

  RemoveTrayIcon();
  DestroyWindows();
  UnregisterClassW(kShortcutClassName, instance_);
  UnregisterClassW(kTargetUrlClassName, instance_);
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
  shortcut_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);

  WNDCLASSEXW target_url_class{};
  target_url_class.cbSize = sizeof(target_url_class);
  target_url_class.hInstance = instance_;
  target_url_class.lpfnWndProc = TargetUrlWindowProc;
  target_url_class.lpszClassName = kTargetUrlClassName;
  target_url_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  target_url_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);

  return RegisterClassExW(&owner_class) && RegisterClassExW(&indicator_class) &&
         RegisterClassExW(&shortcut_class) && RegisterClassExW(&target_url_class);
}

bool Win32Host::CreateWindows() {
  owner_window_ = CreateWindowExW(WS_EX_TOOLWINDOW,
                                  kOwnerClassName,
                                  L"SelectBridge",
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

  indicator_window_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE |
                                          WS_EX_LAYERED,
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
  CloseTargetUrlEditor();
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
  tray_data_.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_SELECT_BRIDGE));
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
  HMENU indicator_menu = CreatePopupMenu();
  HMENU indicator_action_menu = CreatePopupMenu();
  HMENU icon_size_menu = CreatePopupMenu();
  HMENU dot_size_menu = CreatePopupMenu();
  HMENU target_menu = CreatePopupMenu();
  HMENU settings_menu = CreatePopupMenu();
  HMENU language_menu = CreatePopupMenu();
  if (!menu || !trigger_menu || !indicator_menu || !indicator_action_menu ||
      !icon_size_menu || !dot_size_menu || !target_menu || !settings_menu ||
      !language_menu) {
    if (language_menu) DestroyMenu(language_menu);
    if (settings_menu) DestroyMenu(settings_menu);
    if (target_menu) DestroyMenu(target_menu);
    if (dot_size_menu) DestroyMenu(dot_size_menu);
    if (icon_size_menu) DestroyMenu(icon_size_menu);
    if (indicator_action_menu) DestroyMenu(indicator_action_menu);
    if (indicator_menu) DestroyMenu(indicator_menu);
    if (trigger_menu) DestroyMenu(trigger_menu);
    if (menu) DestroyMenu(menu);
    return;
  }

  AppendMenuW(menu,
              MF_STRING,
              kCommandToggleEnabled,
              LocalizedString(enabled_ ? IDS_MENU_PAUSE_FORWARDING
                                        : IDS_MENU_RESUME_FORWARDING)
                  .c_str());
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

  const bool target_locked = !target_override_source_.empty();
  AppendMenuW(target_menu,
              MF_STRING | (target_locked ? MF_GRAYED : MF_ENABLED),
              kCommandTargetGoldendict,
              L"GoldenDict-ng Popup");
  AppendMenuW(target_menu,
              MF_STRING | (target_locked || custom_target_url_.empty() ? MF_GRAYED : MF_ENABLED),
              kCommandTargetCustom,
              LocalizedString(IDS_MENU_CUSTOM_URL).c_str());
  CheckMenuRadioItem(target_menu,
                     kCommandTargetGoldendict,
                     kCommandTargetCustom,
                     target_mode_ == "custom" ? kCommandTargetCustom : kCommandTargetGoldendict,
                     MF_BYCOMMAND);
  AppendMenuW(target_menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(target_menu,
              MF_STRING,
              kCommandSetTargetUrl,
              LocalizedString(IDS_MENU_SET_URL_TEMPLATE).c_str());
  AppendMenuW(menu,
              MF_POPUP,
              reinterpret_cast<UINT_PTR>(target_menu),
              LocalizedString(target_locked ? IDS_MENU_LOOKUP_TARGET_OVERRIDE
                                            : IDS_MENU_LOOKUP_TARGET)
                  .c_str());

  AppendMenuW(trigger_menu,
              MF_STRING,
              kCommandImmediate,
              LocalizedString(IDS_MENU_FORWARD_IMMEDIATELY).c_str());
  AppendMenuW(trigger_menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(trigger_menu, MF_STRING, kCommandIcon,
              LocalizedString(IDS_MENU_SHOW_ICON).c_str());
  AppendMenuW(trigger_menu, MF_STRING, kCommandDot,
              LocalizedString(IDS_MENU_SHOW_DOT).c_str());
  AppendMenuW(trigger_menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(trigger_menu, MF_STRING, kCommandCtrl,
              LocalizedString(IDS_MENU_HOLD_CTRL_TO_FORWARD).c_str());
  AppendMenuW(trigger_menu, MF_STRING, kCommandAlt,
              LocalizedString(IDS_MENU_HOLD_ALT_TO_FORWARD).c_str());
  AppendMenuW(trigger_menu, MF_STRING, kCommandShift,
              LocalizedString(IDS_MENU_HOLD_SHIFT_TO_FORWARD).c_str());
  AppendMenuW(trigger_menu,
              MF_STRING,
              kCommandCustom,
              LocalizedString(custom_shortcut_.empty()
                                  ? IDS_MENU_CUSTOM_SHORTCUT_NOT_SET
                                  : IDS_MENU_CUSTOM_SHORTCUT)
                  .c_str());
  AppendMenuW(trigger_menu, MF_SEPARATOR, 0, nullptr);
  const std::wstring shortcut_label =
      custom_shortcut_.empty()
          ? LocalizedString(IDS_MENU_SET_CUSTOM_SHORTCUT)
          : FormatLocalizedString(IDS_MENU_SET_CUSTOM_SHORTCUT_VALUE,
                                  L"{shortcut}",
                                  Utf8ToWideLocal(custom_shortcut_));
  AppendMenuW(trigger_menu, MF_STRING, kCommandSetShortcut, shortcut_label.c_str());

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
              LocalizedString(IDS_MENU_TRIGGER_MODE).c_str());

  AppendMenuW(indicator_action_menu, MF_STRING, kCommandIndicatorClick,
              LocalizedString(IDS_MENU_CLICK).c_str());
  AppendMenuW(indicator_action_menu, MF_STRING, kCommandIndicatorHover,
              LocalizedString(IDS_MENU_HOVER).c_str());
  CheckMenuRadioItem(indicator_action_menu,
                     kCommandIndicatorClick,
                     kCommandIndicatorHover,
                     indicator_action_ == "hover" ? kCommandIndicatorHover
                                                  : kCommandIndicatorClick,
                     MF_BYCOMMAND);
  AppendMenuW(indicator_menu,
              MF_POPUP,
              reinterpret_cast<UINT_PTR>(indicator_action_menu),
              LocalizedString(IDS_MENU_ACTIVATION).c_str());

  AppendMenuW(icon_size_menu, MF_STRING, kCommandIconSize24, L"24 px");
  AppendMenuW(icon_size_menu, MF_STRING, kCommandIconSize28, L"28 px");
  AppendMenuW(icon_size_menu, MF_STRING, kCommandIconSize32, L"32 px");
  AppendMenuW(icon_size_menu, MF_STRING, kCommandIconSize36, L"36 px");
  AppendMenuW(icon_size_menu, MF_STRING, kCommandIconSize40, L"40 px");
  const UINT checked_icon_size = icon_size_ <= 24   ? kCommandIconSize24
                                 : icon_size_ <= 28 ? kCommandIconSize28
                                 : icon_size_ <= 32 ? kCommandIconSize32
                                 : icon_size_ <= 36 ? kCommandIconSize36
                                                    : kCommandIconSize40;
  CheckMenuRadioItem(icon_size_menu,
                     kCommandIconSize24,
                     kCommandIconSize40,
                     checked_icon_size,
                     MF_BYCOMMAND);
  AppendMenuW(indicator_menu,
              MF_POPUP,
              reinterpret_cast<UINT_PTR>(icon_size_menu),
              LocalizedString(IDS_MENU_ICON_SIZE).c_str());

  AppendMenuW(dot_size_menu, MF_STRING, kCommandDotSize12, L"12 px");
  AppendMenuW(dot_size_menu, MF_STRING, kCommandDotSize16, L"16 px");
  AppendMenuW(dot_size_menu, MF_STRING, kCommandDotSize20, L"20 px");
  AppendMenuW(dot_size_menu, MF_STRING, kCommandDotSize24, L"24 px");
  AppendMenuW(dot_size_menu, MF_STRING, kCommandDotSize28, L"28 px");
  const UINT checked_dot_size = dot_size_ <= 12   ? kCommandDotSize12
                                : dot_size_ <= 16 ? kCommandDotSize16
                                : dot_size_ <= 20 ? kCommandDotSize20
                                : dot_size_ <= 24 ? kCommandDotSize24
                                                  : kCommandDotSize28;
  CheckMenuRadioItem(dot_size_menu,
                     kCommandDotSize12,
                     kCommandDotSize28,
                     checked_dot_size,
                     MF_BYCOMMAND);
  AppendMenuW(indicator_menu,
              MF_POPUP,
              reinterpret_cast<UINT_PTR>(dot_size_menu),
              LocalizedString(IDS_MENU_DOT_SIZE).c_str());

  AppendMenuW(menu,
              MF_POPUP,
              reinterpret_cast<UINT_PTR>(indicator_menu),
              LocalizedString(IDS_MENU_INDICATOR_SETTINGS).c_str());

  AppendMenuW(settings_menu,
              MF_STRING | (auto_start_ ? MF_CHECKED : MF_UNCHECKED),
              kCommandToggleAutoStart,
              LocalizedString(IDS_MENU_START_WITH_WINDOWS).c_str());
  AppendMenuW(language_menu, MF_STRING, kCommandLanguageEnglish,
              LocalizedString(IDS_MENU_ENGLISH).c_str());
  AppendMenuW(language_menu, MF_STRING, kCommandLanguageSimplifiedChinese,
              LocalizedString(IDS_MENU_SIMPLIFIED_CHINESE).c_str());
  CheckMenuRadioItem(language_menu,
                     kCommandLanguageEnglish,
                     kCommandLanguageSimplifiedChinese,
                     ui_language_ == "zh-CN" ? kCommandLanguageSimplifiedChinese
                                             : kCommandLanguageEnglish,
                     MF_BYCOMMAND);
  AppendMenuW(settings_menu,
              MF_POPUP,
              reinterpret_cast<UINT_PTR>(language_menu),
              LocalizedString(IDS_MENU_LANGUAGE).c_str());
  AppendMenuW(settings_menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(settings_menu, MF_STRING, kCommandCheckForUpdates,
              LocalizedString(IDS_MENU_CHECK_FOR_UPDATES).c_str());
  AppendMenuW(settings_menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(settings_menu, MF_STRING, kCommandOpenConfigFile,
              LocalizedString(IDS_MENU_OPEN_CONFIG_FILE).c_str());
  AppendMenuW(settings_menu, MF_STRING, kCommandOpenConfigDirectory,
              LocalizedString(IDS_MENU_OPEN_CONFIG_FOLDER).c_str());
  AppendMenuW(settings_menu, MF_STRING, kCommandReloadConfig,
              LocalizedString(IDS_MENU_RELOAD_CONFIGURATION).c_str());
  AppendMenuW(menu,
              MF_POPUP,
              reinterpret_cast<UINT_PTR>(settings_menu),
              LocalizedString(IDS_MENU_SETTINGS).c_str());
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kCommandExit,
              LocalizedString(IDS_MENU_EXIT).c_str());

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
    case kCommandTargetGoldendict:
      SendEvent("set-target-mode", "goldendict");
      break;
    case kCommandTargetCustom:
      SendEvent("set-target-mode", "custom");
      break;
    case kCommandSetTargetUrl:
      ShowTargetUrlEditor();
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
      if (custom_shortcut_.empty()) {
        ShowShortcutCapture(true);
        break;
      }
      const ShortcutResult result = ApplyShortcut(custom_shortcut_);
      if (result.ok) {
        SendEvent("set-trigger-mode", "custom");
      } else {
        ShowShortcutCapture(true);
        const ShortcutCaptureState failure_state =
            result.error_code == ERROR_INVALID_PARAMETER
                ? ShortcutCaptureState::invalid
                : result.error_code == ERROR_HOTKEY_ALREADY_REGISTERED
                      ? ShortcutCaptureState::conflict
                      : ShortcutCaptureState::error;
        UpdateShortcutCapture(failure_state, custom_shortcut_);
      }
      break;
    }
    case kCommandIndicatorClick:
      SendEvent("set-indicator-action", "click");
      break;
    case kCommandIndicatorHover:
      SendEvent("set-indicator-action", "hover");
      break;
    case kCommandIconSize24: SendEvent("set-icon-size", "24"); break;
    case kCommandIconSize28: SendEvent("set-icon-size", "28"); break;
    case kCommandIconSize32: SendEvent("set-icon-size", "32"); break;
    case kCommandIconSize36: SendEvent("set-icon-size", "36"); break;
    case kCommandIconSize40: SendEvent("set-icon-size", "40"); break;
    case kCommandDotSize12: SendEvent("set-dot-size", "12"); break;
    case kCommandDotSize16: SendEvent("set-dot-size", "16"); break;
    case kCommandDotSize20: SendEvent("set-dot-size", "20"); break;
    case kCommandDotSize24: SendEvent("set-dot-size", "24"); break;
    case kCommandDotSize28: SendEvent("set-dot-size", "28"); break;
    case kCommandSetShortcut:
      ShowShortcutCapture(false);
      break;
    case kCommandToggleAutoStart:
      SendEvent("toggle-auto-start");
      break;
    case kCommandLanguageEnglish:
      SendEvent("set-ui-language", "en-US");
      break;
    case kCommandLanguageSimplifiedChinese:
      SendEvent("set-ui-language", "zh-CN");
      break;
    case kCommandOpenConfigFile:
      SendEvent("open-config-file");
      break;
    case kCommandOpenConfigDirectory:
      SendEvent("open-config-directory");
      break;
    case kCommandReloadConfig:
      SendEvent("reload-config");
      break;
    case kCommandCheckForUpdates:
      SendEvent("check-for-updates");
      break;
    case kCommandExit:
      SendEvent("exit");
      break;
  }
}

void Win32Host::ShowShortcutCapture(bool activate_after_save) {
  if (target_url_window_) {
    if (target_url_save_pending_) {
      ShowWindow(target_url_window_, SW_RESTORE);
      SetForegroundWindow(target_url_window_);
      return;
    }
    CloseTargetUrlEditor();
  }
  if (shortcut_window_) {
    shortcut_activate_after_save_ = shortcut_activate_after_save_ || activate_after_save;
    ShowWindow(shortcut_window_, SW_RESTORE);
    SetForegroundWindow(shortcut_window_);
    SetFocus(shortcut_value_edit_ ? shortcut_value_edit_ : shortcut_window_);
    return;
  }

  shortcut_activate_after_save_ = activate_after_save;
  captured_shortcut_.clear();
  shortcut_preview_ = custom_shortcut_;
  shortcut_capture_state_ = ShortcutCaptureState::current;
  constexpr DWORD extended_style = WS_EX_CONTROLPARENT;
  constexpr DWORD window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
  shortcut_window_ = CreateWindowExW(extended_style,
                                     kShortcutClassName,
                                     LocalizedString(IDS_SHORTCUT_TITLE).c_str(),
                                     window_style,
                                     CW_USEDEFAULT,
                                     CW_USEDEFAULT,
                                     440,
                                     236,
                                     owner_window_,
                                     nullptr,
                                     instance_,
                                     this);
  if (!shortcut_window_) {
    return;
  }

  const UINT dpi = GetWindowDpiCompat(shortcut_window_);
  ResizeAndCenterWindow(shortcut_window_, 400, 176, window_style, extended_style, dpi);
  shortcut_instructions_ = CreateWindowExW(
      0,
      L"STATIC",
      LocalizedString(IDS_SHORTCUT_INSTRUCTIONS).c_str(),
      WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX,
      0,
      0,
      0,
      0,
      shortcut_window_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kShortcutInstructions)),
      instance_,
      nullptr);
  shortcut_field_label_ = CreateWindowExW(
      0,
      L"STATIC",
      LocalizedString(IDS_SHORTCUT_FIELD).c_str(),
      WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX,
      0,
      0,
      0,
      0,
      shortcut_window_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kShortcutFieldLabel)),
      instance_,
      nullptr);
  shortcut_value_frame_ = CreateWindowExW(
      WS_EX_CLIENTEDGE,
      L"STATIC",
      L"",
      WS_CHILD | WS_VISIBLE | SS_WHITERECT | SS_NOTIFY,
      0,
      0,
      0,
      0,
      shortcut_window_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kShortcutValueFrame)),
      instance_,
      nullptr);
  shortcut_value_edit_ = CreateWindowExW(
      0,
      L"EDIT",
      L"",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_CENTER | ES_READONLY | ES_AUTOHSCROLL,
      0,
      0,
      0,
      0,
      shortcut_window_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kShortcutValueEdit)),
      instance_,
      nullptr);
  shortcut_status_label_ = CreateWindowExW(
      0,
      L"STATIC",
      L"",
      WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX,
      0,
      0,
      0,
      0,
      shortcut_window_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kShortcutStatusLabel)),
      instance_,
      nullptr);
  shortcut_remove_button_ = CreateWindowExW(
      0,
      L"BUTTON",
      LocalizedString(IDS_SHORTCUT_REMOVE).c_str(),
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
      0,
      0,
      0,
      0,
      shortcut_window_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kShortcutRemoveButton)),
      instance_,
      nullptr);
  shortcut_cancel_button_ = CreateWindowExW(
      0,
      L"BUTTON",
      LocalizedString(IDS_COMMON_CANCEL).c_str(),
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
      0,
      0,
      0,
      0,
      shortcut_window_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kShortcutCancelButton)),
      instance_,
      nullptr);
  shortcut_save_button_ = CreateWindowExW(
      0,
      L"BUTTON",
      LocalizedString(IDS_COMMON_SAVE).c_str(),
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
      0,
      0,
      0,
      0,
      shortcut_window_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kShortcutSaveButton)),
      instance_,
      nullptr);

  if (!shortcut_instructions_ || !shortcut_field_label_ || !shortcut_value_frame_ ||
      !shortcut_value_edit_ ||
      !shortcut_status_label_ || !shortcut_remove_button_ ||
      !shortcut_cancel_button_ || !shortcut_save_button_) {
    CloseShortcutCapture();
    return;
  }

  RecreateShortcutFonts();
  ApplyShortcutFonts();
  LayoutShortcutCapture();
  UpdateShortcutCapture(ShortcutCaptureState::current, custom_shortcut_);

  ShowWindow(shortcut_window_, SW_SHOW);
  SetForegroundWindow(shortcut_window_);
  SetFocus(shortcut_value_edit_);
}

void Win32Host::CloseShortcutCapture() {
  if (shortcut_window_) {
    DestroyWindow(shortcut_window_);
  }
}

void Win32Host::LayoutShortcutCapture() {
  if (!shortcut_window_) {
    return;
  }
  const UINT dpi = GetWindowDpiCompat(shortcut_window_);
  const auto scaled = [dpi](int value) { return ScaleForDpi(value, dpi); };
  const auto position = [&](HWND control, int x, int y, int width, int height) {
    if (control) {
      SetWindowPos(control,
                   nullptr,
                   scaled(x),
                   scaled(y),
                   scaled(width),
                   scaled(height),
                   SWP_NOACTIVATE | SWP_NOZORDER);
    }
  };
  position(shortcut_instructions_, 24, 10, 352, 22);
  position(shortcut_field_label_, 24, 40, 352, 18);
  const int frame_x = scaled(24);
  const int frame_y = scaled(64);
  const int frame_width = scaled(352);
  const int frame_height = scaled(36);
  SetWindowPos(shortcut_value_frame_, nullptr, frame_x, frame_y, frame_width, frame_height,
               SWP_NOACTIVATE | SWP_NOZORDER);
  const int text_height = MeasureFontHeight(shortcut_value_edit_, shortcut_value_font_, scaled(18));
  const int edit_height = std::min(frame_height - scaled(6), text_height + scaled(6));
  SetWindowPos(shortcut_value_edit_,
               nullptr,
               frame_x + scaled(6),
               frame_y + (frame_height - edit_height) / 2,
               frame_width - scaled(12),
               edit_height,
               SWP_NOACTIVATE | SWP_NOZORDER);
  position(shortcut_status_label_, 24, 106, 352, 18);
  position(shortcut_remove_button_, 24, 134, 80, 30);
  position(shortcut_cancel_button_, 204, 134, 80, 30);
  position(shortcut_save_button_, 296, 134, 80, 30);
}

void Win32Host::RecreateShortcutFonts() {
  if (!shortcut_window_) {
    return;
  }
  if (shortcut_font_) {
    DeleteObject(shortcut_font_);
    shortcut_font_ = nullptr;
  }
  if (shortcut_value_font_) {
    DeleteObject(shortcut_value_font_);
    shortcut_value_font_ = nullptr;
  }

  const UINT dpi = GetWindowDpiCompat(shortcut_window_);
  NONCLIENTMETRICSW metrics{};
  metrics.cbSize = sizeof(metrics);
  using SystemParametersInfoForDpiFunction =
      BOOL(WINAPI*)(UINT, UINT, PVOID, UINT, UINT);
  auto* system_parameters_for_dpi =
      reinterpret_cast<SystemParametersInfoForDpiFunction>(
          GetProcAddress(GetModuleHandleW(L"user32.dll"),
                         "SystemParametersInfoForDpi"));
  const BOOL loaded = system_parameters_for_dpi
                          ? system_parameters_for_dpi(SPI_GETNONCLIENTMETRICS,
                                                      sizeof(metrics),
                                                      &metrics,
                                                      0,
                                                      dpi)
                          : SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,
                                                  sizeof(metrics),
                                                  &metrics,
                                                  0);
  if (!loaded) {
    GetObjectW(GetStockObject(DEFAULT_GUI_FONT), sizeof(metrics.lfMessageFont),
               &metrics.lfMessageFont);
  }

  LOGFONTW body_font = metrics.lfMessageFont;
  LOGFONTW value_font = metrics.lfMessageFont;
  value_font.lfHeight = -MulDiv(11, static_cast<int>(dpi), 72);
  value_font.lfWeight = FW_SEMIBOLD;
  shortcut_font_ = CreateFontIndirectW(&body_font);
  shortcut_value_font_ = CreateFontIndirectW(&value_font);
}

void Win32Host::ApplyShortcutFonts() {
  HFONT body_font = shortcut_font_
                        ? shortcut_font_
                        : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  for (HWND control : {shortcut_instructions_,
                       shortcut_field_label_,
                       shortcut_status_label_,
                       shortcut_remove_button_,
                       shortcut_cancel_button_,
                       shortcut_save_button_}) {
    if (control) {
      SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(body_font), TRUE);
    }
  }
  if (shortcut_value_edit_) {
    HFONT value_font = shortcut_value_font_ ? shortcut_value_font_ : body_font;
    SendMessageW(shortcut_value_edit_,
                 WM_SETFONT,
                 reinterpret_cast<WPARAM>(value_font),
                 TRUE);
  }
}

void Win32Host::UpdateShortcutCapture(ShortcutCaptureState state,
                                      const std::string& preview) {
  shortcut_capture_state_ = state;
  shortcut_preview_ = preview;
  captured_shortcut_ = state == ShortcutCaptureState::valid ? preview : std::string{};
  if (shortcut_value_edit_) {
    const std::wstring value = Utf8ToWideLocal(shortcut_preview_);
    SetWindowTextW(shortcut_value_edit_, value.c_str());
    SendMessageW(shortcut_value_edit_, EM_SETSEL, 0, 0);
  }

  std::wstring status_text =
      LocalizedString(custom_shortcut_.empty()
                          ? IDS_SHORTCUT_NOT_SET
                          : trigger_mode_ == "custom" ? IDS_SHORTCUT_ACTIVE
                                                      : IDS_SHORTCUT_SAVED_INACTIVE);
  switch (shortcut_capture_state_) {
    case ShortcutCaptureState::waiting:
      status_text = LocalizedString(IDS_SHORTCUT_CONTINUE);
      break;
    case ShortcutCaptureState::valid:
      status_text = LocalizedString(IDS_SHORTCUT_AVAILABLE);
      break;
    case ShortcutCaptureState::same:
      status_text = LocalizedString(IDS_SHORTCUT_SAME);
      break;
    case ShortcutCaptureState::conflict:
      status_text = LocalizedString(IDS_SHORTCUT_CONFLICT);
      break;
    case ShortcutCaptureState::invalid:
      status_text = LocalizedString(IDS_SHORTCUT_INVALID);
      break;
    case ShortcutCaptureState::error:
      status_text = LocalizedString(IDS_SHORTCUT_REGISTRATION_FAILED);
      break;
    case ShortcutCaptureState::current:
      break;
  }
  if (shortcut_status_label_) {
    SetWindowTextW(shortcut_status_label_, status_text.c_str());
  }
  if (shortcut_save_button_) {
    EnableWindow(shortcut_save_button_, state == ShortcutCaptureState::valid);
  }
  if (shortcut_remove_button_) {
    EnableWindow(shortcut_remove_button_, !custom_shortcut_.empty());
  }
}

void Win32Host::CaptureShortcutKey(UINT virtual_key) {
  if (IsModifierVirtualKey(virtual_key)) {
    UpdateShortcutCapture(ShortcutCaptureState::waiting, CaptureModifierPreview());
    return;
  }

  if (!HasShortcutModifierDown()) {
    UpdateShortcutCapture(ShortcutCaptureState::invalid, VirtualKeyName(virtual_key));
    return;
  }

  const std::string candidate = CaptureHotkey(virtual_key);
  if (candidate.empty()) {
    UpdateShortcutCapture(ShortcutCaptureState::invalid, VirtualKeyName(virtual_key));
    return;
  }

  const ShortcutResult probe = ProbeShortcut(candidate);
  const std::string preview = probe.normalized.empty() ? candidate : probe.normalized;
  if (!probe.ok) {
    const ShortcutCaptureState failure_state =
        probe.error_code == ERROR_INVALID_PARAMETER
            ? ShortcutCaptureState::invalid
            : probe.error_code == ERROR_HOTKEY_ALREADY_REGISTERED
                  ? ShortcutCaptureState::conflict
                  : ShortcutCaptureState::error;
    UpdateShortcutCapture(failure_state, preview);
    if (failure_state == ShortcutCaptureState::error) {
      SendEvent("native-error", "ShortcutProbe:" + std::to_string(probe.error_code));
    }
    return;
  }

  const ParsedHotkey configured = ParseHotkey(custom_shortcut_);
  if (configured.ok && configured.normalized == probe.normalized) {
    UpdateShortcutCapture(ShortcutCaptureState::same, probe.normalized);
    return;
  }
  UpdateShortcutCapture(ShortcutCaptureState::valid, probe.normalized);
}

void Win32Host::SaveCapturedShortcut() {
  if (shortcut_capture_state_ != ShortcutCaptureState::valid ||
      captured_shortcut_.empty()) {
    return;
  }

  const bool should_register = shortcut_activate_after_save_ || trigger_mode_ == "custom";
  ShortcutResult result = should_register ? ApplyShortcut(captured_shortcut_)
                                          : ProbeShortcut(captured_shortcut_);
  if (!result.ok) {
    const ShortcutCaptureState failure_state =
        result.error_code == ERROR_INVALID_PARAMETER
            ? ShortcutCaptureState::invalid
            : result.error_code == ERROR_HOTKEY_ALREADY_REGISTERED
                  ? ShortcutCaptureState::conflict
                  : ShortcutCaptureState::error;
    UpdateShortcutCapture(failure_state, captured_shortcut_);
    if (failure_state == ShortcutCaptureState::error) {
      SendEvent("native-error", "RegisterHotKey:" + std::to_string(result.error_code));
    }
    return;
  }

  custom_shortcut_ = result.normalized;
  if (!should_register) {
    ClearShortcut();
  }
  SendEvent(shortcut_activate_after_save_ ? "set-custom-shortcut-and-activate"
                                         : "set-custom-shortcut",
            result.normalized);
  CloseShortcutCapture();
}

void Win32Host::RemoveCapturedShortcut() {
  if (custom_shortcut_.empty()) {
    return;
  }
  ClearShortcut();
  custom_shortcut_.clear();
  SendEvent("remove-custom-shortcut");
  CloseShortcutCapture();
}

void Win32Host::ShowTargetUrlEditor() {
  if (shortcut_window_) {
    CloseShortcutCapture();
  }
  if (target_url_window_) {
    ShowWindow(target_url_window_, SW_RESTORE);
    SetForegroundWindow(target_url_window_);
    SetFocus(target_url_edit_ ? target_url_edit_ : target_url_window_);
    return;
  }

  if (target_url_save_pending_) {
    const std::wstring message = LocalizedString(IDS_URL_SAVE_PENDING_MESSAGE);
    const std::wstring title = LocalizedString(IDS_URL_TITLE);
    MessageBoxW(owner_window_, message.c_str(), title.c_str(),
                MB_OK | MB_ICONINFORMATION);
    return;
  }

  constexpr DWORD extended_style = WS_EX_CONTROLPARENT;
  constexpr DWORD window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
  target_url_window_ = CreateWindowExW(extended_style,
                                       kTargetUrlClassName,
                                       LocalizedString(IDS_URL_TITLE).c_str(),
                                       window_style,
                                       CW_USEDEFAULT,
                                       CW_USEDEFAULT,
                                       600,
                                       300,
                                       owner_window_,
                                       nullptr,
                                       instance_,
                                       this);
  if (!target_url_window_) return;

  const UINT dpi = GetWindowDpiCompat(target_url_window_);
  ResizeAndCenterWindow(target_url_window_, 540, 220, window_style, extended_style, dpi);
  const bool locked = !target_override_source_.empty();
  const std::wstring instructions = LocalizedString(
      locked ? IDS_URL_OVERRIDE_INSTRUCTIONS : IDS_URL_INSTRUCTIONS);
  target_url_instructions_ = CreateWindowExW(0, L"STATIC", instructions.c_str(),
      WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
      0, 0, 0, 0, target_url_window_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTargetUrlInstructions)), instance_, nullptr);
  target_url_field_label_ = CreateWindowExW(0, L"STATIC",
      LocalizedString(IDS_URL_FIELD).c_str(),
      WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX,
      0, 0, 0, 0, target_url_window_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTargetUrlFieldLabel)), instance_, nullptr);
  target_url_edit_frame_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"",
      WS_CHILD | WS_VISIBLE | SS_WHITERECT | SS_NOTIFY,
      0, 0, 0, 0, target_url_window_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTargetUrlEditFrame)), instance_, nullptr);
  target_url_edit_ = CreateWindowExW(0, L"EDIT", L"",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | (locked ? ES_READONLY : 0),
      0, 0, 0, 0, target_url_window_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTargetUrlEdit)), instance_, nullptr);
  target_url_status_label_ = CreateWindowExW(0, L"STATIC", L"",
      WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX,
      0, 0, 0, 0, target_url_window_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTargetUrlStatusLabel)), instance_, nullptr);
  target_url_copy_button_ = CreateWindowExW(0, L"BUTTON",
      LocalizedString(IDS_URL_COPY).c_str(),
      WS_CHILD | (locked ? WS_VISIBLE : 0) | WS_TABSTOP | BS_PUSHBUTTON,
      0, 0, 0, 0, target_url_window_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTargetUrlCopyButton)), instance_, nullptr);
  target_url_cancel_button_ = CreateWindowExW(0, L"BUTTON",
      LocalizedString(locked ? IDS_COMMON_CLOSE : IDS_COMMON_CANCEL).c_str(),
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
      0, 0, 0, 0, target_url_window_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTargetUrlCancelButton)), instance_, nullptr);
  target_url_save_button_ = CreateWindowExW(0, L"BUTTON",
      LocalizedString(IDS_COMMON_SAVE).c_str(),
      WS_CHILD | (locked ? 0 : WS_VISIBLE) | WS_TABSTOP | BS_DEFPUSHBUTTON,
      0, 0, 0, 0, target_url_window_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTargetUrlSaveButton)), instance_, nullptr);

  if (!target_url_instructions_ || !target_url_field_label_ || !target_url_edit_frame_ ||
      !target_url_edit_ ||
      !target_url_status_label_ || !target_url_copy_button_ || !target_url_cancel_button_ ||
      !target_url_save_button_) {
    CloseTargetUrlEditor();
    return;
  }
  SendMessageW(target_url_edit_, EM_SETLIMITTEXT, 2048, 0);
  SendMessageW(target_url_edit_, kEditSetCueBannerMessage, TRUE,
               reinterpret_cast<LPARAM>(LocalizedString(IDS_URL_CUE).c_str()));
  const std::wstring current = Utf8ToWideLocal(custom_target_url_);
  SetWindowTextW(target_url_edit_, current.c_str());
  RecreateTargetUrlFont();
  ApplyTargetUrlFont();
  LayoutTargetUrlEditor();
  UpdateTargetUrlValidation();
  ShowWindow(target_url_window_, SW_SHOW);
  SetForegroundWindow(target_url_window_);
  SetFocus(locked ? target_url_copy_button_ : target_url_edit_);
}

void Win32Host::CloseTargetUrlEditor() {
  if (target_url_window_) DestroyWindow(target_url_window_);
}

void Win32Host::LayoutTargetUrlEditor() {
  if (!target_url_window_) return;
  const UINT dpi = GetWindowDpiCompat(target_url_window_);
  const auto scaled = [dpi](int value) { return ScaleForDpi(value, dpi); };
  const auto position = [&](HWND control, int x, int y, int width, int height) {
    if (control) SetWindowPos(control, nullptr, scaled(x), scaled(y), scaled(width), scaled(height),
                              SWP_NOACTIVATE | SWP_NOZORDER);
  };
  position(target_url_instructions_, 24, 12, 492, 40);
  position(target_url_field_label_, 24, 58, 492, 18);
  const int frame_x = scaled(24);
  const int frame_y = scaled(80);
  const int frame_width = scaled(492);
  const int frame_height = scaled(36);
  SetWindowPos(target_url_edit_frame_, nullptr, frame_x, frame_y, frame_width, frame_height,
               SWP_NOACTIVATE | SWP_NOZORDER);
  const int text_height = MeasureFontHeight(target_url_edit_, target_url_font_, scaled(16));
  const int edit_height = std::min(frame_height - scaled(6), text_height + scaled(6));
  SetWindowPos(target_url_edit_,
               nullptr,
               frame_x + scaled(6),
               frame_y + (frame_height - edit_height) / 2,
               frame_width - scaled(12),
               edit_height,
               SWP_NOACTIVATE | SWP_NOZORDER);
  position(target_url_status_label_, 24, 122, 492, 34);
  position(target_url_copy_button_, 252, 170, 80, 30);
  position(target_url_cancel_button_, 344, 170, 80, 30);
  position(target_url_save_button_, 436, 170, 80, 30);
}

void Win32Host::RecreateTargetUrlFont() {
  if (target_url_font_) DeleteObject(target_url_font_);
  target_url_font_ = nullptr;
  NONCLIENTMETRICSW metrics{};
  metrics.cbSize = sizeof(metrics);
  using SystemParametersInfoForDpiFunction = BOOL(WINAPI*)(UINT, UINT, PVOID, UINT, UINT);
  auto* system_parameters_for_dpi = reinterpret_cast<SystemParametersInfoForDpiFunction>(
      GetProcAddress(GetModuleHandleW(L"user32.dll"), "SystemParametersInfoForDpi"));
  const UINT dpi = target_url_window_ ? GetWindowDpiCompat(target_url_window_) : 96;
  const BOOL loaded = system_parameters_for_dpi
                          ? system_parameters_for_dpi(SPI_GETNONCLIENTMETRICS,
                                                      sizeof(metrics), &metrics, 0, dpi)
                          : SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,
                                                  sizeof(metrics), &metrics, 0);
  if (loaded) {
    target_url_font_ = CreateFontIndirectW(&metrics.lfMessageFont);
  }
}

void Win32Host::ApplyTargetUrlFont() {
  HFONT font = target_url_font_ ? target_url_font_
                                : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  for (HWND control : {target_url_instructions_, target_url_field_label_, target_url_edit_,
                       target_url_status_label_, target_url_copy_button_,
                       target_url_cancel_button_, target_url_save_button_}) {
    if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  }
}

void Win32Host::UpdateTargetUrlValidation() {
  if (!target_url_edit_) return;
  if (!target_override_source_.empty()) {
    const std::wstring source = LocalizedString(
        target_override_source_ == "cli" ? IDS_URL_SOURCE_COMMAND_LINE
                                         : IDS_URL_SOURCE_ENVIRONMENT);
    const std::wstring status =
        FormatLocalizedString(IDS_URL_SOURCE, L"{source}", source);
    SetWindowTextW(target_url_status_label_, status.c_str());
    return;
  }
  const int length = GetWindowTextLengthW(target_url_edit_);
  std::wstring value(static_cast<size_t>(length) + 1, L'\0');
  GetWindowTextW(target_url_edit_, value.data(), length + 1);
  value.resize(static_cast<size_t>(length));
  const bool valid = IsValidTargetUrl(value);
  SetWindowTextW(target_url_status_label_,
                 LocalizedString(valid ? IDS_URL_VALID : IDS_URL_INVALID).c_str());
  EnableWindow(target_url_save_button_, valid && !target_url_save_pending_);
}

void Win32Host::SaveTargetUrl() {
  if (!target_url_edit_ || !target_override_source_.empty() || target_url_save_pending_) return;
  const int length = GetWindowTextLengthW(target_url_edit_);
  std::wstring value(static_cast<size_t>(length) + 1, L'\0');
  GetWindowTextW(target_url_edit_, value.data(), length + 1);
  value.resize(static_cast<size_t>(length));
  if (!IsValidTargetUrl(value)) {
    UpdateTargetUrlValidation();
    return;
  }
  target_url_save_pending_ = true;
  pending_target_url_ = WideToUtf8Local(value);
  EnableWindow(target_url_edit_, FALSE);
  EnableWindow(target_url_cancel_button_, FALSE);
  EnableWindow(target_url_save_button_, FALSE);
  SetWindowTextW(target_url_status_label_, LocalizedString(IDS_URL_SAVING).c_str());
  SendEvent("save-target-url", pending_target_url_);
}

void Win32Host::CopyTargetUrl() {
  if (!target_url_edit_ || !OpenClipboard(target_url_window_)) return;
  const int length = GetWindowTextLengthW(target_url_edit_);
  std::wstring value(static_cast<size_t>(length) + 1, L'\0');
  GetWindowTextW(target_url_edit_, value.data(), length + 1);
  EmptyClipboard();
  HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (value.size() + 1) * sizeof(wchar_t));
  if (memory) {
    void* data = GlobalLock(memory);
    if (data) {
      memcpy(data, value.data(), value.size() * sizeof(wchar_t));
      static_cast<wchar_t*>(data)[value.size()] = L'\0';
      GlobalUnlock(memory);
      if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
    } else {
      GlobalFree(memory);
    }
  }
  CloseClipboard();
}

void Win32Host::ApplyTargetUrlSaveResult(bool ok, const std::string& message) {
  target_url_save_pending_ = false;
  if (ok) {
    custom_target_url_ = pending_target_url_;
    pending_target_url_.clear();
    target_mode_ = "custom";
    if (target_url_window_) CloseTargetUrlEditor();
    return;
  }
  pending_target_url_.clear();
  if (!target_url_window_) return;
  EnableWindow(target_url_edit_, TRUE);
  EnableWindow(target_url_cancel_button_, TRUE);
  const std::wstring error = message.empty()
                                 ? LocalizedString(IDS_URL_SAVE_FAILED)
                                 : Utf8ToWideLocal(message);
  SetWindowTextW(target_url_status_label_, error.c_str());
  const int length = GetWindowTextLengthW(target_url_edit_);
  std::wstring value(static_cast<size_t>(length) + 1, L'\0');
  GetWindowTextW(target_url_edit_, value.data(), length + 1);
  value.resize(static_cast<size_t>(length));
  EnableWindow(target_url_save_button_, IsValidTargetUrl(value));
  SetFocus(target_url_edit_);
}

std::wstring Win32Host::LocalizedString(unsigned int resource_id) const {
  return LoadLocalizedString(instance_, resource_id, ui_language_);
}

std::wstring Win32Host::FormatLocalizedString(
    unsigned int resource_id,
    const std::wstring& placeholder,
    const std::wstring& value) const {
  return ReplacePlaceholder(LocalizedString(resource_id), placeholder, value);
}

void Win32Host::RefreshLocalizedWindows() {
  RefreshShortcutWindowText();
  RefreshTargetUrlWindowText();
}

void Win32Host::RefreshShortcutWindowText() {
  if (!shortcut_window_) return;
  SetWindowTextW(shortcut_window_, LocalizedString(IDS_SHORTCUT_TITLE).c_str());
  SetWindowTextW(shortcut_instructions_,
                 LocalizedString(IDS_SHORTCUT_INSTRUCTIONS).c_str());
  SetWindowTextW(shortcut_field_label_, LocalizedString(IDS_SHORTCUT_FIELD).c_str());
  SetWindowTextW(shortcut_remove_button_, LocalizedString(IDS_SHORTCUT_REMOVE).c_str());
  SetWindowTextW(shortcut_cancel_button_, LocalizedString(IDS_COMMON_CANCEL).c_str());
  SetWindowTextW(shortcut_save_button_, LocalizedString(IDS_COMMON_SAVE).c_str());
  UpdateShortcutCapture(shortcut_capture_state_, shortcut_preview_);
  LayoutShortcutCapture();
  RedrawWindow(shortcut_window_, nullptr, nullptr,
               RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

void Win32Host::RefreshTargetUrlWindowText() {
  if (!target_url_window_) return;
  const bool locked = !target_override_source_.empty();
  SetWindowTextW(target_url_window_, LocalizedString(IDS_URL_TITLE).c_str());
  SetWindowTextW(target_url_instructions_,
                 LocalizedString(locked ? IDS_URL_OVERRIDE_INSTRUCTIONS
                                        : IDS_URL_INSTRUCTIONS)
                     .c_str());
  SetWindowTextW(target_url_field_label_, LocalizedString(IDS_URL_FIELD).c_str());
  SetWindowTextW(target_url_copy_button_, LocalizedString(IDS_URL_COPY).c_str());
  SetWindowTextW(target_url_cancel_button_,
                 LocalizedString(locked ? IDS_COMMON_CLOSE : IDS_COMMON_CANCEL).c_str());
  SetWindowTextW(target_url_save_button_, LocalizedString(IDS_COMMON_SAVE).c_str());
  const std::wstring cue = LocalizedString(IDS_URL_CUE);
  SendMessageW(target_url_edit_, kEditSetCueBannerMessage, TRUE,
               reinterpret_cast<LPARAM>(cue.c_str()));
  if (target_url_save_pending_) {
    SetWindowTextW(target_url_status_label_, LocalizedString(IDS_URL_SAVING).c_str());
  } else {
    UpdateTargetUrlValidation();
  }
  LayoutTargetUrlEditor();
  RedrawWindow(target_url_window_, nullptr, nullptr,
               RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
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

  if (indicator_icon_) {
    DestroyIcon(indicator_icon_);
    indicator_icon_ = nullptr;
  }
  if (indicator_style_ == "icon") {
    const int icon_extent = std::max(8, size - std::max(6, size / 4));
    indicator_icon_ = static_cast<HICON>(LoadImageW(instance_,
                                                    MAKEINTRESOURCEW(IDI_SELECT_BRIDGE),
                                                    IMAGE_ICON,
                                                    icon_extent,
                                                    icon_extent,
                                                    LR_DEFAULTCOLOR));
  }

  SetWindowPos(indicator_window_,
               HWND_TOPMOST,
               x,
               y,
               size,
               size,
               SWP_NOACTIVATE);
  PaintIndicator(indicator_window_);
  ShowWindow(indicator_window_, SW_SHOWNOACTIVATE);
}

void Win32Host::PaintIndicator(HWND window) {
  const int size = indicator_size_;
  HDC screen = GetDC(nullptr);
  HDC memory = CreateCompatibleDC(screen);
  BITMAPINFO bitmap_info{};
  bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmap_info.bmiHeader.biWidth = size;
  bitmap_info.bmiHeader.biHeight = -size;
  bitmap_info.bmiHeader.biPlanes = 1;
  bitmap_info.bmiHeader.biBitCount = 32;
  bitmap_info.bmiHeader.biCompression = BI_RGB;
  void* raw_pixels = nullptr;
  HBITMAP bitmap = CreateDIBSection(
      screen, &bitmap_info, DIB_RGB_COLORS, &raw_pixels, nullptr, 0);
  if (!memory || !bitmap || !raw_pixels) {
    if (bitmap) DeleteObject(bitmap);
    if (memory) DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    return;
  }

  HGDIOBJ previous_bitmap = SelectObject(memory, bitmap);
  auto* pixels = static_cast<BgraPixel*>(raw_pixels);
  std::fill_n(pixels, size * size, BgraPixel{});

  const bool icon_style = indicator_style_ == "icon";
  const double radius = icon_style ? std::max(5.0, size * 0.22) : size / 2.0;
  for (int pixel_y = 0; pixel_y < size; ++pixel_y) {
    for (int pixel_x = 0; pixel_x < size; ++pixel_x) {
      const double outer = icon_style
                               ? RoundedRectangleCoverage(
                                     pixel_x, pixel_y, 0.0, 0.0, size, size, radius)
                               : CircleCoverage(pixel_x, pixel_y, size / 2.0, size / 2.0);
      if (outer <= 0.0) {
        continue;
      }
      if (!icon_style) {
        SetPremultipliedPixel(
            &pixels[pixel_y * size + pixel_x], kIndicatorColor, outer, outer);
        continue;
      }

      const double inner = RoundedRectangleCoverage(
          pixel_x,
          pixel_y,
          2.0,
          2.0,
          size - 2.0,
          size - 2.0,
          std::max(3.0, radius - 2.0));
      const double border = std::max(0.0, outer - inner);
      const double alpha = outer;
      BgraPixel& pixel = pixels[pixel_y * size + pixel_x];
      pixel.red = static_cast<std::uint8_t>(std::lround(
          GetRValue(kIndicatorBorderColor) * border +
          GetRValue(kIndicatorBackgroundColor) * inner));
      pixel.green = static_cast<std::uint8_t>(std::lround(
          GetGValue(kIndicatorBorderColor) * border +
          GetGValue(kIndicatorBackgroundColor) * inner));
      pixel.blue = static_cast<std::uint8_t>(std::lround(
          GetBValue(kIndicatorBorderColor) * border +
          GetBValue(kIndicatorBackgroundColor) * inner));
      pixel.alpha = static_cast<std::uint8_t>(std::lround(255.0 * alpha));
    }
  }

  if (icon_style && indicator_icon_) {
    const int icon_extent = std::max(12, size - std::max(10, size / 3));
    const int inset = (size - icon_extent) / 2;
    HDC icon_memory = CreateCompatibleDC(screen);
    void* raw_icon_pixels = nullptr;
    HBITMAP icon_bitmap = CreateDIBSection(
        screen, &bitmap_info, DIB_RGB_COLORS, &raw_icon_pixels, nullptr, 0);
    if (icon_memory && icon_bitmap && raw_icon_pixels) {
      HGDIOBJ previous_icon_bitmap = SelectObject(icon_memory, icon_bitmap);
      auto* icon_pixels = static_cast<BgraPixel*>(raw_icon_pixels);
      std::fill_n(icon_pixels, size * size, BgraPixel{});
      DrawIconEx(icon_memory,
                 inset,
                 inset,
                 indicator_icon_,
                 icon_extent,
                 icon_extent,
                 0,
                 nullptr,
                 DI_NORMAL);
      for (int pixel = 0; pixel < size * size; ++pixel) {
        const double source_alpha = icon_pixels[pixel].alpha / 255.0;
        if (source_alpha <= 0.0) {
          continue;
        }
        const double destination_alpha = pixels[pixel].alpha / 255.0;
        pixels[pixel].red = static_cast<std::uint8_t>(std::clamp(
            icon_pixels[pixel].red + pixels[pixel].red * (1.0 - source_alpha),
            0.0,
            255.0));
        pixels[pixel].green = static_cast<std::uint8_t>(std::clamp(
            icon_pixels[pixel].green + pixels[pixel].green * (1.0 - source_alpha),
            0.0,
            255.0));
        pixels[pixel].blue = static_cast<std::uint8_t>(std::clamp(
            icon_pixels[pixel].blue + pixels[pixel].blue * (1.0 - source_alpha),
            0.0,
            255.0));
        pixels[pixel].alpha = static_cast<std::uint8_t>(std::lround(
            255.0 * (source_alpha + destination_alpha * (1.0 - source_alpha))));
      }
      SelectObject(icon_memory, previous_icon_bitmap);
    }
    if (icon_bitmap) DeleteObject(icon_bitmap);
    if (icon_memory) DeleteDC(icon_memory);
  }

  RECT window_bounds{};
  GetWindowRect(window, &window_bounds);
  POINT destination{window_bounds.left, window_bounds.top};
  POINT source{0, 0};
  SIZE dimensions{size, size};
  BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  const BOOL updated = UpdateLayeredWindow(window,
                                           screen,
                                           &destination,
                                           &dimensions,
                                           memory,
                                           &source,
                                           0,
                                           &blend,
                                           ULW_ALPHA);
  if (!updated) {
    SendEvent("native-error", "UpdateLayeredWindow:" + std::to_string(GetLastError()));
  }

  SelectObject(memory, previous_bitmap);
  DeleteObject(bitmap);
  DeleteDC(memory);
  ReleaseDC(nullptr, screen);
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

Win32Host::ShortcutResult Win32Host::ProbeShortcut(const std::string& shortcut) const {
  ShortcutResult result;
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
  result.ok = true;
  return result;
}

Win32Host::ShortcutResult Win32Host::ApplyShortcut(const std::string& shortcut) {
  ShortcutResult result;
  if (shortcut.empty()) {
    ClearShortcut();
    result.ok = true;
    return result;
  }

  result = ProbeShortcut(shortcut);
  if (!result.ok) {
    return result;
  }

  const ParsedHotkey parsed = ParseHotkey(result.normalized);
  if (hotkey_registered_ && parsed.modifiers == hotkey_modifiers_ &&
      parsed.virtual_key == hotkey_virtual_key_) {
    return result;
  }

  const bool had_registered_hotkey = hotkey_registered_;
  const UINT previous_modifiers = hotkey_modifiers_;
  const UINT previous_virtual_key = hotkey_virtual_key_;
  ClearShortcut();
  if (!RegisterHotKey(owner_window_,
                      kHotkeyId,
                      parsed.modifiers | MOD_NOREPEAT,
                      parsed.virtual_key)) {
    result.ok = false;
    result.error_code = GetLastError();
    if (had_registered_hotkey &&
        RegisterHotKey(owner_window_,
                       kHotkeyId,
                       previous_modifiers | MOD_NOREPEAT,
                       previous_virtual_key)) {
      hotkey_registered_ = true;
      hotkey_modifiers_ = previous_modifiers;
      hotkey_virtual_key_ = previous_virtual_key;
    }
    return result;
  }

  hotkey_registered_ = true;
  hotkey_modifiers_ = parsed.modifiers;
  hotkey_virtual_key_ = parsed.virtual_key;
  custom_shortcut_ = parsed.normalized;
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
      if (lparam == WM_RBUTTONUP || lparam == WM_CONTEXTMENU || lparam == WM_LBUTTONUP) {
        host->ShowTrayMenu();
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
      host->custom_shortcut_ = request->custom_shortcut;
      host->target_mode_ = request->target_mode == "custom" ? "custom" : "goldendict";
      host->custom_target_url_ = request->custom_target_url;
      host->target_override_source_ = request->target_override_source;
      host->ui_language_ = request->ui_language == "zh-CN" ? "zh-CN" : "en-US";
      host->RefreshLocalizedWindows();
      if (host->trigger_mode_ == "custom") {
        if (host->custom_shortcut_.empty()) {
          host->ClearShortcut();
          host->trigger_mode_ = "immediate";
          host->SendEvent("set-trigger-mode", "immediate");
          return 0;
        }
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
    case kTargetUrlSaveResultMessage: {
      std::unique_ptr<TargetSaveResult> request(reinterpret_cast<TargetSaveResult*>(lparam));
      host->ApplyTargetUrlSaveResult(request->ok, request->message);
      return 0;
    }
    case kShowErrorMessage: {
      std::unique_ptr<ErrorRequest> request(reinterpret_cast<ErrorRequest*>(lparam));
      const std::wstring title = Utf8ToWideLocal(request->title);
      const std::wstring text = Utf8ToWideLocal(request->message);
      MessageBoxW(host->target_url_window_ ? host->target_url_window_ : host->owner_window_,
                  text.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
      return 0;
    }
    case kShowInfoMessage: {
      auto* request = reinterpret_cast<DialogRequest*>(lparam);
      const std::wstring title = Utf8ToWideLocal(request->title);
      const std::wstring text = Utf8ToWideLocal(request->message);
      MessageBoxW(host->target_url_window_ ? host->target_url_window_ : host->owner_window_,
                  text.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
      return 0;
    }
    case kConfirmMessage: {
      auto* request = reinterpret_cast<DialogRequest*>(lparam);
      const std::wstring title = Utf8ToWideLocal(request->title);
      const std::wstring text = Utf8ToWideLocal(request->message);
      request->confirmed =
          MessageBoxW(host->target_url_window_ ? host->target_url_window_ : host->owner_window_,
                      text.c_str(), title.c_str(), MB_YESNO | MB_ICONINFORMATION) == IDYES;
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
      host->CloseTargetUrlEditor();
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
      const UINT virtual_key = static_cast<UINT>(wparam);
      const bool has_modifier = HasShortcutModifierDown();
      if (virtual_key == VK_F4 && IsVirtualKeyDown(VK_MENU)) {
        host->CloseShortcutCapture();
        return 0;
      }
      if (virtual_key == VK_ESCAPE && !has_modifier) {
        host->CloseShortcutCapture();
        return 0;
      }
      if (virtual_key == VK_RETURN && !has_modifier) {
        host->SaveCapturedShortcut();
        return 0;
      }
      if (virtual_key == VK_TAB && !has_modifier) {
        HWND focus = GetFocus();
        const HWND controls[] = {host->shortcut_value_edit_,
                                 host->shortcut_remove_button_,
                                 host->shortcut_cancel_button_,
                                 host->shortcut_save_button_};
        size_t current = ARRAYSIZE(controls) - 1;
        for (size_t index = 0; index < ARRAYSIZE(controls); ++index) {
          if (controls[index] == focus) {
            current = index;
            break;
          }
        }
        for (size_t offset = 1; offset <= ARRAYSIZE(controls); ++offset) {
          HWND next = controls[(current + offset) % ARRAYSIZE(controls)];
          if (next && IsWindowVisible(next) && IsWindowEnabled(next)) {
            SetFocus(next);
            break;
          }
        }
        return 0;
      }
      if (virtual_key == VK_SPACE && !has_modifier) {
        HWND focus = GetFocus();
        if (focus == host->shortcut_remove_button_ ||
            focus == host->shortcut_save_button_ ||
            focus == host->shortcut_cancel_button_) {
          SendMessageW(focus, BM_CLICK, 0, 0);
          return 0;
        }
      }
      host->CaptureShortcutKey(virtual_key);
      return 0;
    }
    case WM_GETDLGCODE:
      return DLGC_WANTALLKEYS;
    case WM_COMMAND:
      if (LOWORD(wparam) == kShortcutValueFrame) {
        SetFocus(host->shortcut_value_edit_);
        return 0;
      }
      if (LOWORD(wparam) == kShortcutSaveButton) {
        host->SaveCapturedShortcut();
        return 0;
      }
      if (LOWORD(wparam) == kShortcutCancelButton) {
        host->CloseShortcutCapture();
        return 0;
      }
      if (LOWORD(wparam) == kShortcutRemoveButton) {
        host->RemoveCapturedShortcut();
        return 0;
      }
      break;
    case WM_DPICHANGED: {
      const auto* suggested = reinterpret_cast<RECT*>(lparam);
      SetWindowPos(window,
                   nullptr,
                   suggested->left,
                   suggested->top,
                   suggested->right - suggested->left,
                   suggested->bottom - suggested->top,
                   SWP_NOACTIVATE | SWP_NOZORDER);
      host->RecreateShortcutFonts();
      host->ApplyShortcutFonts();
      host->LayoutShortcutCapture();
      RedrawWindow(window,
                   nullptr,
                   nullptr,
                   RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
      return 0;
    }
    case WM_SETTINGCHANGE:
      host->RecreateShortcutFonts();
      host->ApplyShortcutFonts();
      host->LayoutShortcutCapture();
      RedrawWindow(window,
                   nullptr,
                   nullptr,
                   RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
      return 0;
    case WM_SYSCOLORCHANGE:
    case WM_THEMECHANGED:
      RedrawWindow(window,
                   nullptr,
                   nullptr,
                   RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
      return 0;
    case WM_CLOSE:
      host->CloseShortcutCapture();
      return 0;
    case WM_DESTROY:
      host->shortcut_window_ = nullptr;
      host->shortcut_instructions_ = nullptr;
      host->shortcut_field_label_ = nullptr;
      host->shortcut_value_frame_ = nullptr;
      host->shortcut_value_edit_ = nullptr;
      host->shortcut_status_label_ = nullptr;
      host->shortcut_remove_button_ = nullptr;
      host->shortcut_save_button_ = nullptr;
      host->shortcut_cancel_button_ = nullptr;
      host->captured_shortcut_.clear();
      host->shortcut_preview_.clear();
      host->shortcut_capture_state_ = ShortcutCaptureState::current;
      host->shortcut_activate_after_save_ = false;
      if (host->shortcut_font_) {
        DeleteObject(host->shortcut_font_);
        host->shortcut_font_ = nullptr;
      }
      if (host->shortcut_value_font_) {
        DeleteObject(host->shortcut_value_font_);
        host->shortcut_value_font_ = nullptr;
      }
      return 0;
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32Host::TargetUrlWindowProc(HWND window,
                                               UINT message,
                                               WPARAM wparam,
                                               LPARAM lparam) {
  if (message == WM_NCCREATE) AttachHost(window, lparam);
  Win32Host* host = GetHost(window);
  if (!host) return DefWindowProcW(window, message, wparam, lparam);

  switch (message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
      if ((wparam == VK_F4 && IsVirtualKeyDown(VK_MENU)) ||
          (wparam == VK_ESCAPE && !HasShortcutModifierDown())) {
        if (!host->target_url_save_pending_) {
          host->CloseTargetUrlEditor();
        }
        return 0;
      }
      if (wparam == VK_RETURN) {
        HWND focus = GetFocus();
        if (focus == host->target_url_copy_button_ || focus == host->target_url_cancel_button_ ||
            focus == host->target_url_save_button_) {
          SendMessageW(focus, BM_CLICK, 0, 0);
        } else if (host->target_override_source_.empty()) {
          host->SaveTargetUrl();
        } else {
          host->CopyTargetUrl();
        }
        return 0;
      }
      if (wparam == VK_TAB) {
        HWND controls[] = {host->target_url_edit_, host->target_url_copy_button_,
                           host->target_url_cancel_button_, host->target_url_save_button_};
        HWND focus = GetFocus();
        int current = 0;
        for (int index = 0; index < static_cast<int>(ARRAYSIZE(controls)); ++index) {
          if (controls[index] == focus) current = index;
        }
        const int direction = IsVirtualKeyDown(VK_SHIFT) ? -1 : 1;
        for (int offset = 1; offset <= static_cast<int>(ARRAYSIZE(controls)); ++offset) {
          const int index = (current + direction * offset +
                             static_cast<int>(ARRAYSIZE(controls)) * 2) %
                            static_cast<int>(ARRAYSIZE(controls));
          HWND next = controls[index];
          if (next && IsWindowVisible(next) && IsWindowEnabled(next)) {
            SetFocus(next);
            break;
          }
        }
        return 0;
      }
      break;
    }
    case WM_COMMAND:
      if (LOWORD(wparam) == kTargetUrlEditFrame) {
        SetFocus(host->target_url_edit_);
        return 0;
      }
      if (LOWORD(wparam) == kTargetUrlEdit && HIWORD(wparam) == EN_CHANGE) {
        host->UpdateTargetUrlValidation();
        return 0;
      }
      if (LOWORD(wparam) == kTargetUrlSaveButton) {
        host->SaveTargetUrl();
        return 0;
      }
      if (LOWORD(wparam) == kTargetUrlCopyButton) {
        host->CopyTargetUrl();
        return 0;
      }
      if (LOWORD(wparam) == kTargetUrlCancelButton) {
        if (!host->target_url_save_pending_) {
          host->CloseTargetUrlEditor();
        }
        return 0;
      }
      break;
    case WM_DPICHANGED: {
      const auto* suggested = reinterpret_cast<RECT*>(lparam);
      SetWindowPos(window, nullptr, suggested->left, suggested->top,
                   suggested->right - suggested->left, suggested->bottom - suggested->top,
                   SWP_NOACTIVATE | SWP_NOZORDER);
      host->RecreateTargetUrlFont();
      host->ApplyTargetUrlFont();
      host->LayoutTargetUrlEditor();
      return 0;
    }
    case WM_SETTINGCHANGE:
      host->RecreateTargetUrlFont();
      host->ApplyTargetUrlFont();
      host->LayoutTargetUrlEditor();
      RedrawWindow(window, nullptr, nullptr,
                   RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
      return 0;
    case WM_CLOSE:
      if (!host->target_url_save_pending_) {
        host->CloseTargetUrlEditor();
      }
      return 0;
    case WM_DESTROY:
      host->target_url_window_ = nullptr;
      host->target_url_instructions_ = nullptr;
      host->target_url_field_label_ = nullptr;
      host->target_url_edit_frame_ = nullptr;
      host->target_url_edit_ = nullptr;
      host->target_url_status_label_ = nullptr;
      host->target_url_copy_button_ = nullptr;
      host->target_url_save_button_ = nullptr;
      host->target_url_cancel_button_ = nullptr;
      if (host->target_url_font_) {
        DeleteObject(host->target_url_font_);
        host->target_url_font_ = nullptr;
      }
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
      ValidateRect(window, nullptr);
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
      if (!host->indicator_hover_enabled_) {
        host->EndIndicatorHoverTracking();
        ShowWindow(window, SW_HIDE);
        host->SendEvent("indicator-click");
      }
      return 0;
    case WM_DESTROY:
      host->indicator_window_ = nullptr;
      return 0;
  }

  return DefWindowProcW(window, message, wparam, lparam);
}
