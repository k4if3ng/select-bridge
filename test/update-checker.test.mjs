import assert from 'node:assert/strict';
import { test } from 'node:test';

import {
  checkForUpdates,
  compareStableVersions,
  parseStableVersion,
  RELEASES_PAGE_URL,
} from '../dist/updates/update-checker.js';

function releaseResponse(tagName, htmlUrl = RELEASES_PAGE_URL) {
  return {
    ok: true,
    status: 200,
    async json() {
      return { tag_name: tagName, html_url: htmlUrl };
    },
  };
}

test('compares stable semantic versions numerically', () => {
  assert.deepEqual(parseStableVersion('v1.10.0'), { major: 1, minor: 10, patch: 0 });
  assert.equal(compareStableVersions(parseStableVersion('1.10.0'), parseStableVersion('1.9.9')), 1);
  assert.equal(compareStableVersions(parseStableVersion('1.2.0'), parseStableVersion('1.2.0')), 0);
  assert.equal(parseStableVersion('v1.2.0-beta.1'), undefined);
});

test('reports a newer stable release and sends GitHub API headers', async () => {
  let request;
  const result = await checkForUpdates('1.2.0', {
    fetchImpl: async (url, init) => {
      request = { url, init };
      return releaseResponse(
        'v1.10.0',
        'https://github.com/k4if3ng/select-bridge/releases/tag/v1.10.0',
      );
    },
  });

  assert.deepEqual(result, {
    status: 'update-available',
    currentVersion: '1.2.0',
    latestVersion: '1.10.0',
    releaseUrl: 'https://github.com/k4if3ng/select-bridge/releases/tag/v1.10.0',
  });
  assert.match(request.url, /api\.github\.com\/repos\/k4if3ng\/select-bridge\/releases\/latest$/);
  assert.equal(request.init.headers.Accept, 'application/vnd.github+json');
  assert.equal(request.init.headers['User-Agent'], 'SelectBridge/1.2.0');
});

test('reports up to date when the published release is not newer', async () => {
  const same = await checkForUpdates('1.2.0', {
    fetchImpl: async () => releaseResponse('v1.2.0'),
  });
  const developmentBuild = await checkForUpdates('1.3.0', {
    fetchImpl: async () => releaseResponse('v1.2.0'),
  });

  assert.equal(same.status, 'up-to-date');
  assert.equal(developmentBuild.status, 'up-to-date');
});

test('falls back to the trusted releases page for an unexpected response URL', async () => {
  const result = await checkForUpdates('1.2.0', {
    fetchImpl: async () => releaseResponse('v1.3.0', 'https://example.com/download'),
  });

  assert.equal(result.status, 'update-available');
  assert.equal(result.releaseUrl, RELEASES_PAGE_URL);
});

test('rejects HTTP failures and malformed release tags', async () => {
  await assert.rejects(
    checkForUpdates('1.2.0', {
      fetchImpl: async () => ({
        ok: false,
        status: 403,
        async json() {
          return {};
        },
      }),
    }),
    /HTTP 403/,
  );

  await assert.rejects(
    checkForUpdates('1.2.0', {
      fetchImpl: async () => releaseResponse('latest'),
    }),
    /invalid tag/,
  );
});
