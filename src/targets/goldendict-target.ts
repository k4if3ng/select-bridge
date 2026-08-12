import { openExternalUrl } from '../utils/open-external-url.js';

import type { TranslationTarget } from './types.js';

export class GoldenDictTarget implements TranslationTarget {
  readonly id = 'goldendict-ng';
  readonly name = 'Goldendict-ng';

  constructor(private readonly nativeOpenExternalUrl?: (url: string) => boolean) {}

  async translate(text: string): Promise<void> {
    const url = `goldendict://${encodeURIComponent(text)}?target=popup`;
    if (this.nativeOpenExternalUrl?.(url)) {
      return;
    }
    await openExternalUrl(url);
  }
}
