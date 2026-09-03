export const UI_LANGUAGES = ['en-US', 'zh-CN'] as const;
export type UiLanguage = (typeof UI_LANGUAGES)[number];

interface UiMessages {
  queryTargetTitle: string;
  customUrlRequired: string;
  switchTargetFailedTitle: string;
  switchTargetFailed: string;
  runtimeUrlOverride: string;
  urlTemplateTooLong: string;
  urlTemplateInvalid: string;
  saveUrlFailed: string;
  openConfigFailedTitle: string;
  openConfigFailed: string;
  openPathFailed: string;
  reloadConfigFailedTitle: string;
  reloadConfigFailed: string;
  autoStartTitle: string;
  autoStartUnsupported: string;
  autoStartUpdateFailed: string;
  saveAutoStartFailedTitle: string;
  saveAutoStartFailed: string;
  saveConfigFailedTitle: string;
  saveConfigFailed: string;
  saveLanguageFailedTitle: string;
  saveLanguageFailed: string;
  updateCheckTitle: string;
  updateAvailable: string;
  upToDate: string;
  updateCheckFailedTitle: string;
  updateCheckFailed: string;
  openReleaseFailedTitle: string;
  openReleaseFailed: string;
  detailsLabel: string;
}

const MESSAGES: Record<UiLanguage, UiMessages> = {
  'en-US': {
    queryTargetTitle: 'Lookup target',
    customUrlRequired: 'Set a valid custom URL first.',
    switchTargetFailedTitle: 'Could not switch lookup target',
    switchTargetFailed: 'SelectBridge could not save the lookup target.',
    runtimeUrlOverride: 'The current URL is overridden at runtime and cannot be changed from the tray.',
    urlTemplateTooLong: 'The URL template cannot exceed 2,048 characters.',
    urlTemplateInvalid:
      'The URL template must start with a valid URI scheme, contain no whitespace or control characters, and contain exactly one {text} placeholder.',
    saveUrlFailed: 'SelectBridge could not save the URL template.',
    openConfigFailedTitle: 'Could not open configuration',
    openConfigFailed: 'SelectBridge could not open the configuration path.',
    openPathFailed: 'The system could not open:\n{path}',
    reloadConfigFailedTitle: 'Could not reload configuration',
    reloadConfigFailed: 'SelectBridge could not reload the configuration file.',
    autoStartTitle: 'Start at sign-in',
    autoStartUnsupported: 'The current platform host does not support start-at-sign-in management.',
    autoStartUpdateFailed: 'Windows could not update the start-at-sign-in setting.',
    saveAutoStartFailedTitle: 'Could not save start-at-sign-in setting',
    saveAutoStartFailed: 'SelectBridge could not save the start-at-sign-in setting.',
    saveConfigFailedTitle: 'Could not save configuration',
    saveConfigFailed: 'SelectBridge could not save the configuration file.',
    saveLanguageFailedTitle: 'Could not change interface language',
    saveLanguageFailed: 'SelectBridge could not save the interface language.',
    updateCheckTitle: 'Check for updates',
    updateAvailable:
      'A new version is available.\n\nCurrent version: {current}\nLatest version: {latest}\n\nOpen GitHub Releases?',
    upToDate: 'SelectBridge is up to date.\n\nCurrent version: {current}',
    updateCheckFailedTitle: 'Could not check for updates',
    updateCheckFailed: 'SelectBridge could not check GitHub Releases for updates.',
    openReleaseFailedTitle: 'Could not open download page',
    openReleaseFailed: 'The system could not open GitHub Releases.',
    detailsLabel: 'Details',
  },
  'zh-CN': {
    queryTargetTitle: '查询目标',
    customUrlRequired: '请先设置有效的自定义 URL。',
    switchTargetFailedTitle: '切换查询目标失败',
    switchTargetFailed: 'SelectBridge 未能保存查询目标。',
    runtimeUrlOverride: '当前 URL 由运行参数覆盖，无法从托盘修改。',
    urlTemplateTooLong: 'URL 模板不能超过 2048 个字符。',
    urlTemplateInvalid:
      'URL 模板必须以合法 URI scheme 开头，不能包含空白或控制字符，并且必须且只能包含一个 {text}。',
    saveUrlFailed: 'SelectBridge 未能保存 URL 模板。',
    openConfigFailedTitle: '打开配置失败',
    openConfigFailed: 'SelectBridge 未能打开配置路径。',
    openPathFailed: '系统未能打开：\n{path}',
    reloadConfigFailedTitle: '重新加载配置失败',
    reloadConfigFailed: 'SelectBridge 未能重新加载配置文件。',
    autoStartTitle: '登录时自动启动',
    autoStartUnsupported: '当前平台宿主不支持管理登录时自动启动。',
    autoStartUpdateFailed: 'Windows 未能更新登录时自动启动设置。',
    saveAutoStartFailedTitle: '保存登录时自动启动设置失败',
    saveAutoStartFailed: 'SelectBridge 未能保存登录时自动启动设置。',
    saveConfigFailedTitle: '保存配置失败',
    saveConfigFailed: 'SelectBridge 未能保存配置文件。',
    saveLanguageFailedTitle: '切换界面语言失败',
    saveLanguageFailed: 'SelectBridge 未能保存界面语言。',
    updateCheckTitle: '检查更新',
    updateAvailable:
      '发现新版本。\n\n当前版本：{current}\n最新版本：{latest}\n\n是否打开 GitHub Releases？',
    upToDate: 'SelectBridge 已是最新版本。\n\n当前版本：{current}',
    updateCheckFailedTitle: '检查更新失败',
    updateCheckFailed: 'SelectBridge 未能从 GitHub Releases 检查更新。',
    openReleaseFailedTitle: '打开下载页面失败',
    openReleaseFailed: '系统未能打开 GitHub Releases。',
    detailsLabel: '详细信息',
  },
};

export function isUiLanguage(value: unknown): value is UiLanguage {
  return typeof value === 'string' && (UI_LANGUAGES as readonly string[]).includes(value);
}

export function resolveSupportedUiLanguage(locale: string): UiLanguage {
  try {
    const parsed = new Intl.Locale(locale).maximize();
    if (parsed.language === 'zh' && parsed.script === 'Hans') {
      return 'zh-CN';
    }
  } catch {
    // Invalid or unavailable locale names fall back to the default language.
  }
  return 'en-US';
}

export function uiMessage(language: UiLanguage, key: keyof UiMessages): string {
  return MESSAGES[language][key];
}

export function formatUiMessage(
  language: UiLanguage,
  key: keyof UiMessages,
  values: Readonly<Record<string, string>>,
): string {
  let result = uiMessage(language, key);
  for (const [name, value] of Object.entries(values)) {
    result = result.replaceAll(`{${name}}`, value);
  }
  return result;
}

export function withErrorDetails(
  language: UiLanguage,
  summaryKey: keyof UiMessages,
  detail: string,
): string {
  return `${uiMessage(language, summaryKey)}\n\n${uiMessage(language, 'detailsLabel')}: ${detail}`;
}
