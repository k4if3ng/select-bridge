import assert from 'node:assert/strict';
import { mkdtemp, readFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { test } from 'node:test';

import {
  ConfigStore,
  DEFAULT_CONFIG,
  DEFAULT_TARGET_URL_TEMPLATE,
  isTargetUrlTemplate,
  loadRuntimeOptions,
} from '../dist/config.js';

test('validates URL templates and runtime overrides', () => {
  assert.equal(isTargetUrlTemplate(DEFAULT_TARGET_URL_TEMPLATE), true);
  assert.equal(isTargetUrlTemplate('youdao://query?word={text}'), true);
  assert.equal(isTargetUrlTemplate('youdao://query'), false);
  assert.equal(isTargetUrlTemplate('youdao://{text}/{text}'), false);
  assert.deepEqual(loadRuntimeOptions(['--target-url=youdao://query?word={text}']).targetUrlTemplate, 'youdao://query?word={text}');
});

test('serializes concurrent config saves and leaves valid JSON', async () => {
  const directory = await mkdtemp(join(tmpdir(), 'select-bridge-config-'));
  const path = join(directory, 'config.json');
  const store = new ConfigStore(path);

  try {
    const first = { ...DEFAULT_CONFIG, enabled: false };
    const second = { ...DEFAULT_CONFIG, enabled: true };
    await Promise.all([store.save(first), store.save(second)]);
    await store.flush();

    const saved = JSON.parse(await readFile(path, 'utf8'));
    assert.equal(saved.enabled, true);
    assert.equal(saved.targetUrlTemplate, DEFAULT_TARGET_URL_TEMPLATE);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});
