import { spawn } from 'node:child_process';
import { createRequire } from 'node:module';
import { basename } from 'node:path';

import type { SelectionHookConstructor, TextSelectionData } from 'selection-hook';

const require = createRequire(import.meta.url);
const SelectionHook = require('selection-hook') as SelectionHookConstructor;

const MAX_TEXT_LENGTH = getPositiveInteger('SELECTION_FORWARD_MAX_LENGTH', 200);
const DEDUPE_WINDOW_MS = getPositiveInteger('SELECTION_FORWARD_DEDUPE_MS', 800);

let lastForwardedText = '';
let lastForwardedAt = 0;

const hook = new SelectionHook();

hook.on('text-selection', ({ text, programName, method }: TextSelectionData) => {
  const selectedText = normalizeSelection(text);

  if (!selectedText || isGoldenDict(programName)) {
    return;
  }

  if (selectedText.length > MAX_TEXT_LENGTH) {
    console.warn(
      `[skip] 选区长度为 ${selectedText.length}，超过限制 ${MAX_TEXT_LENGTH}。` +
        ' 可通过 SELECTION_FORWARD_MAX_LENGTH 调整。',
    );
    return;
  }

  const now = Date.now();
  if (selectedText === lastForwardedText && now - lastForwardedAt < DEDUPE_WINDOW_MS) {
    return;
  }

  lastForwardedText = selectedText;
  lastForwardedAt = now;
  lookupInGoldenDict(selectedText, programName, method);
});

hook.on('error', (error: Error) => {
  console.error('[selection-hook]', error);
});

const started = hook.start({
  enableClipboard: true,
});

if (!started) {
  console.error('[error] selection-hook 启动失败。');
  hook.cleanup();
  process.exitCode = 1;
} else {
  console.log('选词转发已启动。选择文字即可在 Goldendict-ng 弹窗查词。');
  console.log('查询协议：goldendict://<选词>?target=popup');
  console.log('按 Ctrl+C 退出。');
}

function normalizeSelection(text: string): string {
  return text.replace(/\s+/g, ' ').trim();
}

function lookupInGoldenDict(
  text: string,
  programName: string,
  method: TextSelectionData['method'],
): void {
  const url = `goldendict://${encodeURIComponent(text)}?target=popup`;
  openExternalUrl(url);

  const source = programName ? `，来源：${programName}` : '';
  console.log(`[lookup] ${JSON.stringify(text)}${source}（检测方式：${describeMethod(method)}）\n         ${url}`);
}

function openExternalUrl(url: string): void {
  const candidates = getUrlOpenCommands(url);
  tryOpenCommand(candidates, 0);
}

function getUrlOpenCommands(url: string): OpenCommand[] {
  switch (process.platform) {
    case 'win32':
      return [{ command: 'rundll32.exe', args: ['url.dll,FileProtocolHandler', url] }];
    case 'darwin':
      return [{ command: 'open', args: [url] }];
    default:
      return [
        { command: 'xdg-open', args: [url] },
        { command: 'gio', args: ['open', url] },
      ];
  }
}

function tryOpenCommand(candidates: OpenCommand[], index: number): void {
  const candidate = candidates[index];
  if (!candidate) {
    console.error(
      '[error] 无法调用系统 URL 处理器。请确认当前桌面环境能够打开 goldendict:// 地址。',
    );
    return;
  }

  const child = spawn(candidate.command, candidate.args, {
    detached: true,
    stdio: 'ignore',
    windowsHide: true,
  });

  let failedToStart = false;
  child.once('error', (error: NodeJS.ErrnoException) => {
    failedToStart = true;
    if (error.code === 'ENOENT' && index + 1 < candidates.length) {
      tryOpenCommand(candidates, index + 1);
      return;
    }

    reportUrlOpenError(error.message);
  });

  child.once('exit', (code) => {
    if (failedToStart || code === 0 || code === null) {
      return;
    }

    if (index + 1 < candidates.length) {
      tryOpenCommand(candidates, index + 1);
      return;
    }

    reportUrlOpenError(`${candidate.command} 退出码 ${code}`);
  });

  child.unref();
}

function reportUrlOpenError(detail: string): void {
  console.error(
    `[error] 无法打开 Goldendict-ng 查询协议：${detail}\n` +
      '请先启动一次 Goldendict-ng，使系统注册 goldendict:// 协议。',
  );
}

function isGoldenDict(programName: string): boolean {
  return /goldendict/i.test(basename(programName));
}

function describeMethod(method: TextSelectionData['method']): string {
  const names: Record<number, string> = {
    [SelectionHook.SelectionMethod.NONE]: 'None',
    [SelectionHook.SelectionMethod.UIA]: 'UI Automation',
    [SelectionHook.SelectionMethod.ACCESSIBLE]: 'IAccessible',
    [SelectionHook.SelectionMethod.AXAPI]: 'Accessibility API',
    [SelectionHook.SelectionMethod.ATSPI]: 'AT-SPI',
    [SelectionHook.SelectionMethod.PRIMARY]: 'Primary selection',
    [SelectionHook.SelectionMethod.CLIPBOARD]: 'Clipboard',
  };

  return names[method] ?? `Unknown (${method})`;
}

function getPositiveInteger(name: string, fallback: number): number {
  const value = Number.parseInt(process.env[name] ?? '', 10);
  return Number.isSafeInteger(value) && value > 0 ? value : fallback;
}

function shutdown(signal: NodeJS.Signals): void {
  console.log(`\n收到 ${signal}，正在停止选词监听…`);
  hook.stop();
  hook.cleanup();
  process.exit(0);
}

interface OpenCommand {
  command: string;
  args: string[];
}

process.once('SIGINT', () => shutdown('SIGINT'));
process.once('SIGTERM', () => shutdown('SIGTERM'));
