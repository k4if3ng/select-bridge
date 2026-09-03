export const RELEASES_PAGE_URL = 'https://github.com/k4if3ng/select-bridge/releases/latest';

const LATEST_RELEASE_API_URL =
  'https://api.github.com/repos/k4if3ng/select-bridge/releases/latest';
const DEFAULT_TIMEOUT_MS = 10_000;

export interface StableVersion {
  major: number;
  minor: number;
  patch: number;
}

interface FetchResponse {
  ok: boolean;
  status: number;
  json(): Promise<unknown>;
}

export type UpdateFetch = (
  url: string,
  init: {
    headers: Readonly<Record<string, string>>;
    signal: AbortSignal;
  },
) => Promise<FetchResponse>;

export interface UpdateCheckOptions {
  fetchImpl?: UpdateFetch;
  timeoutMs?: number;
  signal?: AbortSignal;
}

export type UpdateCheckResult =
  | {
      status: 'update-available';
      currentVersion: string;
      latestVersion: string;
      releaseUrl: string;
    }
  | {
      status: 'up-to-date';
      currentVersion: string;
      latestVersion: string;
    };

export async function checkForUpdates(
  currentVersion: string,
  options: UpdateCheckOptions = {},
): Promise<UpdateCheckResult> {
  const current = parseStableVersion(currentVersion);
  if (!current) {
    throw new Error(`Invalid current version: ${currentVersion}`);
  }

  const controller = new AbortController();
  const abortFromCaller = (): void => controller.abort(options.signal?.reason);
  if (options.signal?.aborted) {
    abortFromCaller();
  } else {
    options.signal?.addEventListener('abort', abortFromCaller, { once: true });
  }
  const timeout = setTimeout(
    () => controller.abort(),
    Math.max(1, options.timeoutMs ?? DEFAULT_TIMEOUT_MS),
  );
  const fetchImpl: UpdateFetch =
    options.fetchImpl ?? ((url, init) => globalThis.fetch(url, init));

  try {
    const response = await fetchImpl(LATEST_RELEASE_API_URL, {
      headers: {
        Accept: 'application/vnd.github+json',
        'User-Agent': `SelectBridge/${currentVersion}`,
      },
      signal: controller.signal,
    });
    if (!response.ok) {
      throw new Error(`GitHub Releases returned HTTP ${response.status}`);
    }

    const payload = await response.json();
    const release = readLatestRelease(payload);
    const latest = parseStableVersion(release.tagName);
    if (!latest) {
      throw new Error(`GitHub Releases returned an invalid tag: ${release.tagName}`);
    }

    const latestVersion = formatStableVersion(latest);
    if (compareStableVersions(latest, current) > 0) {
      return {
        status: 'update-available',
        currentVersion,
        latestVersion,
        releaseUrl: release.releaseUrl,
      };
    }

    return {
      status: 'up-to-date',
      currentVersion,
      latestVersion,
    };
  } finally {
    clearTimeout(timeout);
    options.signal?.removeEventListener('abort', abortFromCaller);
  }
}

export function parseStableVersion(value: string): StableVersion | undefined {
  const match = /^v?(\d+)\.(\d+)\.(\d+)$/.exec(value);
  if (!match) {
    return undefined;
  }

  const parts = match.slice(1).map(Number);
  if (parts.some((part) => !Number.isSafeInteger(part))) {
    return undefined;
  }
  return { major: parts[0]!, minor: parts[1]!, patch: parts[2]! };
}

export function compareStableVersions(left: StableVersion, right: StableVersion): number {
  for (const key of ['major', 'minor', 'patch'] as const) {
    if (left[key] < right[key]) return -1;
    if (left[key] > right[key]) return 1;
  }
  return 0;
}

function formatStableVersion(version: StableVersion): string {
  return `${version.major}.${version.minor}.${version.patch}`;
}

function readLatestRelease(payload: unknown): { tagName: string; releaseUrl: string } {
  if (!payload || typeof payload !== 'object') {
    throw new Error('GitHub Releases returned an invalid response');
  }

  const record = payload as Record<string, unknown>;
  if (typeof record.tag_name !== 'string') {
    throw new Error('GitHub Releases response is missing tag_name');
  }

  return {
    tagName: record.tag_name,
    releaseUrl: isTrustedReleaseUrl(record.html_url) ? record.html_url : RELEASES_PAGE_URL,
  };
}

function isTrustedReleaseUrl(value: unknown): value is string {
  if (typeof value !== 'string') {
    return false;
  }
  try {
    const url = new URL(value);
    return (
      url.protocol === 'https:' &&
      url.hostname === 'github.com' &&
      url.pathname.startsWith('/k4if3ng/select-bridge/releases/')
    );
  } catch {
    return false;
  }
}
