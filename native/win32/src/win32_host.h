#pragma once

#include <node_api.h>
#include <windows.h>
#include <shellapi.h>

#include <atomic>
#include <string>

class Win32Host {
 public:
  struct ShortcutResult {
    bool ok = false;
    DWORD error_code = 0;
    std::string normalized;
  };

  Win32Host(napi_env env, napi_value callback);
  ~Win32Host();

  bool IsValid() const;
  bool Start();
  void Stop();

  bool UpdateTray(bool enabled,
                  const std::string& trigger_mode,
                  bool auto_start,
                  const std::string& indicator_action,
                  int icon_size,
                  int dot_size,
                  const std::string& custom_shortcut,
                  const std::string& target_mode,
                  const std::string& custom_target_url,
                  const std::string& target_override_source);
  bool ShowIndicator(int x,
                     int y,
                     const std::string& style,
                     int size,
                     bool hover_enabled,
                     unsigned int hover_delay_ms);
  bool HideIndicator();
  ShortcutResult RegisterShortcut(const std::string& shortcut);
  bool CompleteTargetUrlSave(bool ok, const std::string& message);
  bool ShowError(const std::string& title, const std::string& message);

  static bool SetAutoStart(bool enabled,
                           const std::wstring& executable_path,
                           const std::wstring& arguments_text);
  static bool OpenExternalUrl(const std::wstring& url);
  static bool OpenPath(const std::wstring& path);

 private:
  struct IndicatorRequest;
  struct TrayStateRequest;
  struct EventPayload;
  struct ShortcutRequest;
  struct TargetSaveResult;
  struct ErrorRequest;
  enum class ShortcutCaptureState {
    current,
    waiting,
    valid,
    same,
    conflict,
    invalid,
    error,
  };

  static DWORD WINAPI ThreadEntry(void* parameter);
  static LRESULT CALLBACK OwnerWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK IndicatorWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK ShortcutWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK TargetUrlWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static void CallJs(napi_env env, napi_value callback, void* context, void* data);

  DWORD ThreadMain();
  bool RegisterWindowClasses();
  bool CreateWindows();
  void DestroyWindows();

  bool AddTrayIcon();
  void RemoveTrayIcon();
  void ShowTrayMenu();
  void HandleTrayCommand(unsigned int command);
  void ShowShortcutCapture(bool activate_after_save);
  void CloseShortcutCapture();
  void LayoutShortcutCapture();
  void RecreateShortcutFonts();
  void ApplyShortcutFonts();
  void UpdateShortcutCapture(ShortcutCaptureState state,
                             const std::string& preview = {});
  void CaptureShortcutKey(UINT virtual_key);
  void SaveCapturedShortcut();
  void RemoveCapturedShortcut();
  void ShowTargetUrlEditor();
  void CloseTargetUrlEditor();
  void LayoutTargetUrlEditor();
  void RecreateTargetUrlFont();
  void ApplyTargetUrlFont();
  void UpdateTargetUrlValidation();
  void SaveTargetUrl();
  void CopyTargetUrl();
  void ApplyTargetUrlSaveResult(bool ok, const std::string& message);

  void ApplyIndicator(const IndicatorRequest& request);
  void PaintIndicator(HWND window);
  void BeginIndicatorHoverTracking();
  void EndIndicatorHoverTracking();
  ShortcutResult ProbeShortcut(const std::string& shortcut) const;
  ShortcutResult ApplyShortcut(const std::string& shortcut);
  void ClearShortcut();

  void SendEvent(const std::string& type, const std::string& value = {});
  void ReleaseThreadsafeFunction(napi_threadsafe_function_release_mode mode);

  napi_env env_ = nullptr;
  napi_threadsafe_function threadsafe_function_ = nullptr;

  HANDLE thread_ = nullptr;
  HANDLE ready_event_ = nullptr;
  DWORD thread_id_ = 0;
  std::atomic<bool> start_succeeded_{false};
  std::atomic<bool> stopping_{false};

  HINSTANCE instance_ = nullptr;
  HWND owner_window_ = nullptr;
  HWND indicator_window_ = nullptr;
  HWND shortcut_window_ = nullptr;
  HWND target_url_window_ = nullptr;
  HWND target_url_instructions_ = nullptr;
  HWND target_url_field_label_ = nullptr;
  HWND target_url_edit_ = nullptr;
  HWND target_url_status_label_ = nullptr;
  HWND target_url_copy_button_ = nullptr;
  HWND target_url_save_button_ = nullptr;
  HWND target_url_cancel_button_ = nullptr;
  HFONT target_url_font_ = nullptr;
  HWND shortcut_instructions_ = nullptr;
  HWND shortcut_field_label_ = nullptr;
  HWND shortcut_value_edit_ = nullptr;
  HWND shortcut_status_label_ = nullptr;
  HWND shortcut_remove_button_ = nullptr;
  HWND shortcut_save_button_ = nullptr;
  HWND shortcut_cancel_button_ = nullptr;
  HFONT shortcut_font_ = nullptr;
  HFONT shortcut_value_font_ = nullptr;
  NOTIFYICONDATAW tray_data_{};
  bool tray_added_ = false;
  UINT taskbar_created_message_ = 0;

  bool enabled_ = true;
  bool auto_start_ = false;
  std::string trigger_mode_ = "immediate";
  std::string indicator_action_ = "click";
  int icon_size_ = 32;
  int dot_size_ = 16;
  std::string custom_shortcut_ = "Ctrl+Alt+G";
  std::string target_mode_ = "goldendict";
  std::string custom_target_url_;
  std::string target_override_source_;
  bool target_url_save_pending_ = false;
  std::string pending_target_url_;
  std::string captured_shortcut_;
  std::string shortcut_preview_;
  ShortcutCaptureState shortcut_capture_state_ = ShortcutCaptureState::current;
  bool shortcut_activate_after_save_ = false;
  bool hotkey_registered_ = false;
  UINT hotkey_modifiers_ = 0;
  UINT hotkey_virtual_key_ = 0;

  std::string indicator_style_ = "icon";
  int indicator_size_ = 40;
  HICON indicator_icon_ = nullptr;
  bool indicator_hover_enabled_ = false;
  bool indicator_tracking_ = false;
  unsigned int indicator_hover_delay_ms_ = 450;
};
