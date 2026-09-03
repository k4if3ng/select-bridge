import assert from 'node:assert/strict';
import { test } from 'node:test';

import { UrlTarget } from '../dist/targets/url-target.js';

test('encodes selected text and uses the native opener when handled', async () => {
  let openedUrl;
  let template = 'youdao://query?word={text}';
  const target = new UrlTarget(() => template, (url) => {
    openedUrl = url;
    return true;
  });

  await target.translate('中文 & C++');

  assert.equal(openedUrl, 'youdao://query?word=%E4%B8%AD%E6%96%87%20%26%20C%2B%2B');

  template = 'goldendict://{text}?target=popup';
  await target.translate('next');
  assert.equal(openedUrl, 'goldendict://next?target=popup');
});
