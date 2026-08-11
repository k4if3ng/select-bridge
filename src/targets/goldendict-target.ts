import { openExternalUrl } from '../utils/open-external-url.js';

import type { TranslationTarget } from './types.js';

export class GoldenDictTarget implements TranslationTarget {
  readonly id = 'goldendict-ng';
  readonly name = 'Goldendict-ng';

  async translate(text: string): Promise<void> {
    await openExternalUrl(`goldendict://${encodeURIComponent(text)}?target=popup`);
  }
}
