# SelectBridge

English | [简体中文](README.zh-CN.md)

Select text in a supported desktop application and forward it to a configurable URL target. The default target opens a [GoldenDict-ng](https://github.com/xiaoyifang/goldendict-ng) popup through `goldendict://`.

```text
Selected text → SelectBridge → URL template → target application
```

SelectBridge does not provide dictionaries or translation services. Windows uses a lightweight native system-tray host; Windows, macOS, and Linux can run the headless host. The project does not depend on Electron, WebView, or Tauri.

## Features

- Global text-selection monitoring through [`selection-hook`](https://github.com/0xfullex/selection-hook)
- Immediate, modifier-key, or custom-shortcut forwarding
- Windows-only floating icon and dot indicators with click or hover activation
- GoldenDict-ng popup and custom URL templates
- Compact Windows tray menu with native settings dialogs
- English and Simplified Chinese Windows UI
- Windows Setup and Portable packages for x64 and ARM64
- Duplicate, rapid-selection, and maximum-length protection

## Install

### Windows packages

Download the package for your architecture from [GitHub Releases](../../releases/latest):

| Package | Description |
| --- | --- |
| `SelectBridge-<version>-windows-<arch>-setup.exe` | Per-user installer with Start menu and uninstall entries |
| `SelectBridge-<version>-windows-<arch>-portable.zip` | Portable directory; extract and run without installation |

`<arch>` is `x64` or `arm64`. Keep every file in the Portable directory together; moving only `SelectBridge.exe` will break the application.

Install and start GoldenDict-ng at least once before using the default target so that Windows registers the `goldendict://` protocol.

## Usage

1. Start SelectBridge.
2. Select text in a supported application.
3. SelectBridge forwards the text according to the configured trigger mode.

The default `immediate` mode forwards automatically. Modifier and custom-shortcut modes wait for `Ctrl`, `Alt`, `Shift`, or a configured key combination. The Windows native host also provides floating `icon` and `dot` modes with click or hover activation.

## Windows tray settings

Left-clicking or right-clicking the tray icon opens the same context menu:

```text
Enable selection forwarding
───────────────────────────
Lookup target >
Trigger mode >
Indicator settings >
Settings >
───────────────────────────
Exit
```

The submenus include:

```text
Lookup target >
  GoldenDict-ng Popup
  Custom URL
  Set URL template…

Trigger mode >
  Forward immediately
  Show icon / Show dot
  Hold Ctrl / Alt / Shift to forward
  Custom shortcut
  Set custom shortcut…

Indicator settings >
  Activation > Click / Hover
  Icon size >
  Dot size >

Settings >
  Start at sign-in
  Language > English / 简体中文
  Check for updates…
  Open config file
  Open config folder
  Reload configuration
```

Settings are saved automatically. English (`en-US`) is the default and fallback UI language. On the first run, a Simplified Chinese system initializes `zh-CN`; other systems initialize `en-US`. The chosen value is then persisted and no longer follows later operating-system language changes.

Language changes take effect immediately in the tray menu and any open settings dialog. The supported languages are:

- English (`en-US`)
- 简体中文 (`zh-CN`)

Select **Settings → Check for updates…** to compare the installed version with the latest stable GitHub Release. If a newer version is available, SelectBridge can open the official download page; it never downloads or installs updates automatically.

## URL templates

Open **Lookup target → Set URL template…** to configure another application or service. A template must:

- start with a valid URI scheme;
- contain exactly one `{text}` placeholder;
- contain no whitespace or control characters;
- be no longer than 2,048 characters.

Selected text replaces `{text}` after `encodeURIComponent` encoding. Saving a valid template immediately selects **Custom URL**. Switching back to GoldenDict-ng does not delete the saved template.

## Configuration

Configuration paths:

- Setup: `%APPDATA%\select-bridge\config.json`
- Portable: `<portable-directory>\data\config.json`
- Other platforms: `$XDG_CONFIG_HOME/select-bridge/config.json` or `~/.config/select-bridge/config.json`

Use `SELECT_BRIDGE_CONFIG` to select another configuration path. Interface language is stored in configuration schema 10:

```json
{
  "schemaVersion": 10,
  "uiLanguage": "en-US"
}
```

Existing configurations without `uiLanguage` are upgraded once using the current system language. Invalid language values fall back to `en-US`.

## Development

```shell
pnpm install
pnpm build
pnpm build:native
pnpm test
```

Run the cross-platform headless host with `pnpm start -- --host=headless`. Building the Windows native host additionally requires the matching Node.js architecture, Windows SDK, Visual Studio C++ tools, and Python available as `python`.

Technical documentation:

- [Architecture](docs/ARCHITECTURE.md)
- [Headless host](docs/HEADLESS.md)
- [Development](docs/DEVELOPMENT.md)
- [Windows implementation](docs/WINDOWS.md)

## License

[MIT](LICENSE)
