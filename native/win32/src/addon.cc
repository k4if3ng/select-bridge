#include "win32_host.h"

#include <node_api.h>

#include <memory>
#include <string>

namespace {

Win32Host* g_host = nullptr;
napi_env g_env = nullptr;

napi_value CreateBoolean(napi_env env, bool value) {
  napi_value result = nullptr;
  napi_get_boolean(env, value, &result);
  return result;
}

napi_value CreateUndefined(napi_env env) {
  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

napi_value CreateShortcutResult(napi_env env, const Win32Host::ShortcutResult& registration) {
  napi_value result = nullptr;
  napi_value ok = nullptr;
  napi_value error_code = nullptr;
  napi_value normalized = nullptr;
  napi_create_object(env, &result);
  napi_get_boolean(env, registration.ok, &ok);
  napi_create_uint32(env, registration.error_code, &error_code);
  napi_create_string_utf8(
      env, registration.normalized.c_str(), registration.normalized.size(), &normalized);
  napi_set_named_property(env, result, "ok", ok);
  napi_set_named_property(env, result, "errorCode", error_code);
  napi_set_named_property(env, result, "normalized", normalized);
  return result;
}

void ThrowLastError(napi_env env, const char* message) {
  napi_throw_error(env, nullptr, message);
}

bool GetBoolean(napi_env env, napi_value value, bool* result) {
  return napi_get_value_bool(env, value, result) == napi_ok;
}

bool GetInt32OrNull(napi_env env, napi_value value, int* result) {
  napi_valuetype type = napi_undefined;
  if (napi_typeof(env, value, &type) != napi_ok) {
    return false;
  }
  if (type == napi_null || type == napi_undefined) {
    *result = INT_MIN;
    return true;
  }
  int32_t parsed = 0;
  if (napi_get_value_int32(env, value, &parsed) != napi_ok) {
    return false;
  }
  *result = parsed;
  return true;
}

bool GetUint32(napi_env env, napi_value value, unsigned int* result) {
  uint32_t parsed = 0;
  if (napi_get_value_uint32(env, value, &parsed) != napi_ok) {
    return false;
  }
  *result = parsed;
  return true;
}

bool GetUtf8(napi_env env, napi_value value, std::string* result) {
  size_t size = 0;
  if (napi_get_value_string_utf8(env, value, nullptr, 0, &size) != napi_ok) {
    return false;
  }
  std::string buffer(size + 1, '\0');
  size_t written = 0;
  if (napi_get_value_string_utf8(env, value, buffer.data(), size + 1, &written) != napi_ok) {
    return false;
  }
  buffer.resize(written);
  *result = std::move(buffer);
  return true;
}

std::wstring Utf8ToWide(const std::string& value) {
  if (value.empty()) {
    return {};
  }
  const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
  std::wstring result(size, L'\0');
  MultiByteToWideChar(
      CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
  return result;
}

void Cleanup(void* data) {
  auto* host = static_cast<Win32Host*>(data);
  if (host == g_host) {
    host->Stop();
    delete host;
    g_host = nullptr;
    g_env = nullptr;
  }
}

napi_value Start(napi_env env, napi_callback_info info) {
  size_t count = 1;
  napi_value arguments[1]{};
  napi_get_cb_info(env, info, &count, arguments, nullptr, nullptr);

  napi_valuetype type = napi_undefined;
  if (count != 1 || napi_typeof(env, arguments[0], &type) != napi_ok || type != napi_function) {
    ThrowLastError(env, "start(callback) requires a function");
    return nullptr;
  }

  if (g_host) {
    return CreateBoolean(env, true);
  }

  auto host = std::make_unique<Win32Host>(env, arguments[0]);
  if (!host->IsValid() || !host->Start()) {
    return CreateBoolean(env, false);
  }

  g_env = env;
  g_host = host.release();
  napi_add_env_cleanup_hook(env, Cleanup, g_host);
  return CreateBoolean(env, true);
}

napi_value Stop(napi_env env, napi_callback_info) {
  if (g_host) {
    napi_remove_env_cleanup_hook(env, Cleanup, g_host);
    g_host->Stop();
    delete g_host;
    g_host = nullptr;
    g_env = nullptr;
  }
  return CreateUndefined(env);
}

napi_value UpdateTray(napi_env env, napi_callback_info info) {
  size_t count = 7;
  napi_value arguments[7]{};
  napi_get_cb_info(env, info, &count, arguments, nullptr, nullptr);

  bool enabled = true;
  bool auto_start = false;
  std::string trigger_mode;
  std::string indicator_action;
  int icon_size = 32;
  int dot_size = 16;
  std::string custom_shortcut;
  if (count != 7 || !GetBoolean(env, arguments[0], &enabled) ||
      !GetUtf8(env, arguments[1], &trigger_mode) ||
      !GetBoolean(env, arguments[2], &auto_start) ||
      !GetUtf8(env, arguments[3], &indicator_action) ||
      !GetInt32OrNull(env, arguments[4], &icon_size) ||
      !GetInt32OrNull(env, arguments[5], &dot_size) ||
      !GetUtf8(env, arguments[6], &custom_shortcut)) {
    ThrowLastError(env, "updateTray received invalid arguments");
    return nullptr;
  }

  return CreateBoolean(
      env,
      g_host && g_host->UpdateTray(enabled,
                                   trigger_mode,
                                   auto_start,
                                   indicator_action,
                                   icon_size,
                                   dot_size,
                                   custom_shortcut));
}

napi_value ShowIndicator(napi_env env, napi_callback_info info) {
  size_t count = 6;
  napi_value arguments[6]{};
  napi_get_cb_info(env, info, &count, arguments, nullptr, nullptr);

  int x = INT_MIN;
  int y = INT_MIN;
  std::string style;
  int size = 40;
  bool hover_enabled = false;
  unsigned int hover_delay_ms = 450;
  if (count != 6 || !GetInt32OrNull(env, arguments[0], &x) ||
      !GetInt32OrNull(env, arguments[1], &y) || !GetUtf8(env, arguments[2], &style) ||
      !GetInt32OrNull(env, arguments[3], &size) ||
      !GetBoolean(env, arguments[4], &hover_enabled) ||
      !GetUint32(env, arguments[5], &hover_delay_ms)) {
    ThrowLastError(env, "showIndicator received invalid arguments");
    return nullptr;
  }

  return CreateBoolean(
      env,
      g_host &&
          g_host->ShowIndicator(x, y, style, size, hover_enabled, hover_delay_ms));
}

napi_value HideIndicator(napi_env env, napi_callback_info) {
  return CreateBoolean(env, g_host && g_host->HideIndicator());
}

napi_value RegisterShortcut(napi_env env, napi_callback_info info) {
  size_t count = 1;
  napi_value arguments[1]{};
  napi_get_cb_info(env, info, &count, arguments, nullptr, nullptr);

  std::string shortcut;
  if (count != 1 || !GetUtf8(env, arguments[0], &shortcut)) {
    ThrowLastError(env, "registerShortcut(shortcut) requires a string");
    return nullptr;
  }

  if (!g_host) {
    Win32Host::ShortcutResult result;
    result.error_code = ERROR_INVALID_WINDOW_HANDLE;
    return CreateShortcutResult(env, result);
  }
  return CreateShortcutResult(env, g_host->RegisterShortcut(shortcut));
}

napi_value SetAutoStart(napi_env env, napi_callback_info info) {
  size_t count = 3;
  napi_value arguments[3]{};
  napi_get_cb_info(env, info, &count, arguments, nullptr, nullptr);

  bool enabled = false;
  std::string executable_path;
  std::string arguments_text;
  if (count != 3 || !GetBoolean(env, arguments[0], &enabled) ||
      !GetUtf8(env, arguments[1], &executable_path) ||
      !GetUtf8(env, arguments[2], &arguments_text)) {
    ThrowLastError(env, "setAutoStart received invalid arguments");
    return nullptr;
  }

  const bool result = Win32Host::SetAutoStart(
      enabled, Utf8ToWide(executable_path), Utf8ToWide(arguments_text));
  return CreateBoolean(env, result);
}

napi_value OpenExternalUrl(napi_env env, napi_callback_info info) {
  size_t count = 1;
  napi_value arguments[1]{};
  napi_get_cb_info(env, info, &count, arguments, nullptr, nullptr);

  std::string url;
  if (count != 1 || !GetUtf8(env, arguments[0], &url)) {
    ThrowLastError(env, "openExternalUrl(url) requires a string");
    return nullptr;
  }

  return CreateBoolean(env, Win32Host::OpenExternalUrl(Utf8ToWide(url)));
}

napi_value Initialize(napi_env env, napi_value exports) {
  const napi_property_descriptor properties[] = {
      {"start", nullptr, Start, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"stop", nullptr, Stop, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"updateTray", nullptr, UpdateTray, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"showIndicator", nullptr, ShowIndicator, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"hideIndicator", nullptr, HideIndicator, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"registerShortcut", nullptr, RegisterShortcut, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setAutoStart", nullptr, SetAutoStart, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"openExternalUrl", nullptr, OpenExternalUrl, nullptr, nullptr, nullptr, napi_default, nullptr},
  };
  napi_define_properties(env, exports, sizeof(properties) / sizeof(properties[0]), properties);
  return exports;
}

}  // namespace

NAPI_MODULE(NODE_GYP_MODULE_NAME, Initialize)
