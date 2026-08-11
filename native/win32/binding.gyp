{
  "targets": [
    {
      "target_name": "selection_forward_win32",
      "sources": [
        "src/addon.cc",
        "src/win32_host.cc"
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
        "advapi32.lib"
      ],
      "msvs_settings": {
        "VCCLCompilerTool": {
          "AdditionalOptions": ["/std:c++20", "/EHsc"]
        }
      }
    }
  ]
}
