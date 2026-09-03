import assert from 'node:assert/strict';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
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
  assert.equal(isTargetUrlTemplate(`custom://${'a'.repeat(2048)}{text}`), false);
  const previousEnvironmentTarget = process.env.SELECT_BRIDGE_TARGET_URL;
  process.env.SELECT_BRIDGE_TARGET_URL = 'environment://lookup/{text}';
  try {
    const runtime = loadRuntimeOptions(['--target-url=youdao://query?word={text}']);
    assert.equal(runtime.targetUrlOverride, 'youdao://query?word={text}');
    assert.equal(runtime.targetUrlOverrideSource, 'cli');
    const environmentRuntime = loadRuntimeOptions([]);
    assert.equal(environmentRuntime.targetUrlOverride, 'environment://lookup/{text}');
    assert.equal(environmentRuntime.targetUrlOverrideSource, 'environment');
  } finally {
    if (previousEnvironmentTarget === undefined) {
      delete process.env.SELECT_BRIDGE_TARGET_URL;
    } else {
      process.env.SELECT_BRIDGE_TARGET_URL = previousEnvironmentTarget;
    }
  }
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
    assert.equal(saved.schemaVersion, 9);
    assert.equal(saved.targetMode, 'goldendict');
    assert.equal(saved.customTargetUrlTemplate, '');
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test('migrates schema 8 target URL settings', async () => {
  const directory = await mkdtemp(join(tmpdir(), 'select-bridge-migration-'));
  const path = join(directory, 'config.json');
  const store = new ConfigStore(path);
  try {
    await writeFile(path, JSON.stringify({
      ...DEFAULT_CONFIG,
      schemaVersion: 8,
      targetMode: undefined,
      customTargetUrlTemplate: undefined,
      targetUrlTemplate: 'youdao://query?word={text}',
    }));
    const migrated = await store.readPersistent();
    assert.equal(migrated.schemaVersion, 9);
    assert.equal(migrated.targetMode, 'custom');
    assert.equal(migrated.customTargetUrlTemplate, 'youdao://query?word={text}');
    assert.equal('targetUrlTemplate' in migrated, false);

    await writeFile(path, JSON.stringify({ schemaVersion: 8, targetUrlTemplate: DEFAULT_TARGET_URL_TEMPLATE }));
    const defaultMigration = await store.readPersistent();
    assert.equal(defaultMigration.targetMode, 'goldendict');
    assert.equal(defaultMigration.customTargetUrlTemplate, '');
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test('strict persistent reads reject invalid JSON without overwriting it', async () => {
  const directory = await mkdtemp(join(tmpdir(), 'select-bridge-invalid-'));
  const path = join(directory, 'config.json');
  const store = new ConfigStore(path);
  try {
    await writeFile(path, '{ invalid json', 'utf8');
    await assert.rejects(store.readPersistent());
    assert.equal(await readFile(path, 'utf8'), '{ invalid json');
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test('merges one tray field with external edits and removes the legacy target field', async () => {
  const directory = await mkdtemp(join(tmpdir(), 'select-bridge-merge-'));
  const path = join(directory, 'config.json');
  const store = new ConfigStore(path);
  try {
    await writeFile(path, JSON.stringify({
      ...DEFAULT_CONFIG,
      enabled: false,
      externalNote: 'keep me',
      targetUrlTemplate: 'legacy://{text}',
    }), 'utf8');
    const updated = await store.mergePersistent({ dotSize: 24 });
    const saved = JSON.parse(await readFile(path, 'utf8'));
    assert.equal(updated.enabled, false);
    assert.equal(updated.dotSize, 24);
    assert.equal(saved.externalNote, 'keep me');
    assert.equal(saved.enabled, false);
    assert.equal(saved.dotSize, 24);
    assert.equal('targetUrlTemplate' in saved, false);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});
