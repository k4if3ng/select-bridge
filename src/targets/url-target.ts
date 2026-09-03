import { openExternalUrl } from '../utils/open-external-url.js';

import type { TranslationTarget } from './types.js';

const TEXT_PLACEHOLDER = '{text}';

export class UrlTarget implements TranslationTarget {
  readonly id = 'url';
  readonly name = 'Configured URL';

  constructor(
    private readonly getTemplate: () => string,
    private readonly nativeOpenExternalUrl?: (url: string) => boolean,
  ) {}

  async translate(text: string): Promise<void> {
    const url = this.getTemplate().replace(TEXT_PLACEHOLDER, encodeURIComponent(text));
    if (this.nativeOpenExternalUrl?.(url)) {
      return;
    }
    await openExternalUrl(url);
  }
}
