export interface TranslationTarget {
  readonly id: string;
  readonly name: string;
  translate(text: string): Promise<void>;
}
