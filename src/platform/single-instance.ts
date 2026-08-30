import { createHash } from 'node:crypto';
import { connect, createServer, type Server } from 'node:net';
import { userInfo } from 'node:os';

import type { DistributionMode } from '../distribution.js';

const INSTANCE_PROTOCOL_VERSION = 1;
const RETRY_DELAY_MS = 50;
const NOTIFY_TIMEOUT_MS = 750;

export interface SingleInstanceGuard {
  readonly isPrimary: boolean;
  readonly activeDistribution?: DistributionMode;
  close(): Promise<void>;
}

export async function acquireSingleInstance(
  distribution: DistributionMode,
): Promise<SingleInstanceGuard> {
  if (process.platform !== 'win32') {
    return createNoopGuard();
  }

  const endpoint = getWindowsPipeName();
  for (let attempt = 0; attempt < 2; attempt += 1) {
    const server = createInstanceServer(distribution);
    try {
      await listen(server, endpoint);
      server.unref();
      return createPrimaryGuard(server);
    } catch (error: unknown) {
      server.removeAllListeners();
      if (!isAddressInUse(error)) {
        throw error;
      }

      const activeDistribution = await notifyPrimaryInstance(endpoint, distribution);
      if (activeDistribution) {
        return createSecondaryGuard(activeDistribution);
      }

      if (attempt === 0) {
        await delay(RETRY_DELAY_MS);
      }
    }
  }

  // The endpoint still exists but did not answer. Fail closed so a stalled
  // primary instance cannot turn into two global selection hooks.
  return createSecondaryGuard();
}

function createInstanceServer(distribution: DistributionMode): Server {
  return createServer((socket) => {
    socket.setEncoding('utf8');
    socket.setTimeout(NOTIFY_TIMEOUT_MS, () => socket.destroy());
    socket.once('data', (source) => {
      const requestedDistribution = parseDistribution(source);
      const sourceLabel = requestedDistribution ?? 'unknown';
      console.log(`[instance] 已阻止 ${sourceLabel} 启动重复实例。`);
    });
    socket.end(distribution);
  });
}

function listen(server: Server, endpoint: string): Promise<void> {
  return new Promise((resolve, reject) => {
    const onError = (error: Error): void => {
      server.off('listening', onListening);
      reject(error);
    };
    const onListening = (): void => {
      server.off('error', onError);
      resolve();
    };

    server.once('error', onError);
    server.once('listening', onListening);
    server.listen(endpoint);
  });
}

function notifyPrimaryInstance(
  endpoint: string,
  distribution: DistributionMode,
): Promise<DistributionMode | undefined> {
  return new Promise((resolve) => {
    let settled = false;
    let response = '';
    const socket = connect(endpoint);
    const timer = setTimeout(() => finish(undefined), NOTIFY_TIMEOUT_MS);

    const finish = (activeDistribution: DistributionMode | undefined): void => {
      if (settled) {
        return;
      }
      settled = true;
      clearTimeout(timer);
      socket.destroy();
      resolve(activeDistribution);
    };

    socket.setEncoding('utf8');
    socket.unref();
    socket.once('connect', () => socket.end(distribution));
    socket.on('data', (chunk) => {
      response += chunk;
    });
    socket.once('end', () => finish(parseDistribution(response)));
    socket.once('error', () => finish(undefined));
  });
}

function getWindowsPipeName(): string {
  let username = process.env.USERNAME ?? 'default';
  try {
    username = userInfo().username || username;
  } catch {
    // Environment fallback keeps startup available in restricted accounts.
  }

  const identity = `${process.env.USERDOMAIN ?? ''}\\${username}`.toLowerCase();
  const userScope = createHash('sha256').update(identity).digest('hex').slice(0, 16);
  return `\\\\.\\pipe\\selection-forward-${userScope}-v${INSTANCE_PROTOCOL_VERSION}`;
}

function parseDistribution(value: string): DistributionMode | undefined {
  const normalized = value.trim();
  return normalized === 'development' || normalized === 'setup' || normalized === 'portable'
    ? normalized
    : undefined;
}

function isAddressInUse(error: unknown): error is NodeJS.ErrnoException {
  return error instanceof Error && 'code' in error && error.code === 'EADDRINUSE';
}

function createPrimaryGuard(server: Server): SingleInstanceGuard {
  let closed = false;
  return {
    isPrimary: true,
    async close(): Promise<void> {
      if (closed) {
        return;
      }
      closed = true;
      await new Promise<void>((resolve) => server.close(() => resolve()));
    },
  };
}

function createSecondaryGuard(activeDistribution?: DistributionMode): SingleInstanceGuard {
  return {
    isPrimary: false,
    activeDistribution,
    async close(): Promise<void> {},
  };
}

function createNoopGuard(): SingleInstanceGuard {
  return {
    isPrimary: true,
    async close(): Promise<void> {},
  };
}

function delay(milliseconds: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}
