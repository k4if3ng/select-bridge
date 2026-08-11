{
  "targets": [
    {
      "target_name": "selection_forward_win32_ui",
      "sources": [
        "src/addon.cc",
        "src/win32_host.cc",
        "resources.rc"
      ],
      "defines": [
        "NAPI_VERSION=8",
        "UNICODE",
        "_UNICODE",
        "WIN32_LEAN_AND_MEAN",
        "NOMINMAX"
      ],
      "libraries": [
        "user32.lib",
        "shell32.lib",
        "gdi32.lib",
        "advapi32.lib",
        "comdlg32.lib"
      ],
      "msvs_settings": {
        "VCCLCompilerTool": {
          "AdditionalOptions": ["/std:c++20", "/EHsc"]
        }
      }
    }
  ]
}
