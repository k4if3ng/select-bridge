#include <windows.h>

#include <string>

namespace {

std::wstring GetExecutableDirectory() {
  std::wstring path(MAX_PATH, L'\0');
  DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  while (length == path.size() && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    path.resize(path.size() * 2);
    length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  }
  if (length == 0 || length >= path.size()) {
    return {};
  }

  path.resize(length);
  const std::wstring::size_type separator = path.find_last_of(L"\\/");
  return separator == std::wstring::npos ? std::wstring{} : path.substr(0, separator);
}

std::wstring QuoteArgument(const std::wstring& value) {
  return L"\"" + value + L"\"";
}

}  // namespace

int RunLauncher(PWSTR command_line) {
  const std::wstring directory = GetExecutableDirectory();
  if (directory.empty()) {
    return ERROR_PATH_NOT_FOUND;
  }

  const std::wstring executable = directory + L"\\SelectionForward.exe";
  if (GetFileAttributesW(executable.c_str()) == INVALID_FILE_ATTRIBUTES) {
    MessageBoxW(nullptr,
                L"找不到 SelectionForward.exe。请保持两个 EXE 位于同一目录。",
                L"Selection Forward",
                MB_OK | MB_ICONERROR);
    return ERROR_FILE_NOT_FOUND;
  }

  std::wstring child_command = QuoteArgument(executable) + L" --tray --silent";
  if (command_line && *command_line) {
    child_command += L" ";
    child_command += command_line;
  }

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  const BOOL started = CreateProcessW(executable.c_str(),
                                      child_command.data(),
                                      nullptr,
                                      nullptr,
                                      FALSE,
                                      CREATE_NO_WINDOW,
                                      nullptr,
                                      directory.c_str(),
                                      &startup,
                                      &process);
  if (!started) {
    const DWORD error = GetLastError();
    const std::wstring message =
        L"无法启动 SelectionForward.exe。Windows 错误码：" + std::to_wstring(error);
    MessageBoxW(nullptr, message.c_str(), L"Selection Forward", MB_OK | MB_ICONERROR);
    return static_cast<int>(error);
  }

  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return 0;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR command_line, int) {
  return RunLauncher(command_line);
}

int main() {
  return RunLauncher(nullptr);
}
