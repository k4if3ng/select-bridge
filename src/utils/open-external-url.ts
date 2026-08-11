import { spawn } from 'node:child_process';

interface OpenCommand {
  command: string;
  args: string[];
}

export async function openExternalUrl(url: string): Promise<void> {
  const candidates = getUrlOpenCommands(url);
  let lastError: Error | undefined;

  for (const candidate of candidates) {
    try {
      await runOpenCommand(candidate);
      return;
    } catch (error: unknown) {
      lastError = error instanceof Error ? error : new Error(String(error));
    }
  }

  throw lastError ?? new Error('无法调用系统 URL 处理器');
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

function runOpenCommand(candidate: OpenCommand): Promise<void> {
  return new Promise((resolve, reject) => {
    const child = spawn(candidate.command, candidate.args, {
      detached: true,
      stdio: 'ignore',
      windowsHide: true,
    });

    let settled = false;
    child.once('error', (error) => {
      if (!settled) {
        settled = true;
        reject(error);
      }
    });
    child.once('exit', (code) => {
      if (settled) {
        return;
      }
      settled = true;
      if (code === 0 || code === null) {
        resolve();
      } else {
        reject(new Error(`${candidate.command} 退出码 ${code}`));
      }
    });
    child.unref();
  });
}
