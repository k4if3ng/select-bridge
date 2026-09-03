import assert from 'node:assert/strict';
import { test } from 'node:test';

import { UrlTarget } from '../dist/targets/url-target.js';

test('encodes selected text and uses the native opener when handled', async () => {
  let openedUrl;
  const target = new UrlTarget('youdao://query?word={text}', (url) => {
    openedUrl = url;
    return true;
  });

  await target.translate('中文 & C++');

  assert.equal(openedUrl, 'youdao://query?word=%E4%B8%AD%E6%96%87%20%26%20C%2B%2B');
});
