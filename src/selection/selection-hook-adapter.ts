import { createRequire } from 'node:module';

import type {
  KeyboardEventData,
  SelectionHookConstructor,
  SelectionHookInstance,
  TextSelectionData,
} from 'selection-hook';

import type { KeyEvent, SelectionEvent } from './types.js';

const require = createRequire(import.meta.url);
const SelectionHook = require('selection-hook') as SelectionHookConstructor;

export interface SelectionHookCallbacks {
  onSelection(event: SelectionEvent): void;
  onKeyDown(event: KeyEvent): void;
  onError(error: Error): void;
}

export class SelectionHookAdapter {
  private readonly hook: SelectionHookInstance;

  constructor(private readonly callbacks: SelectionHookCallbacks) {
    this.hook = new SelectionHook();
    this.hook.on('text-selection', (data) => this.handleSelection(data));
    this.hook.on('key-down', (data) => this.handleKeyDown(data));
    this.hook.on('error', (error) => this.callbacks.onError(error));
  }

  start(): boolean {
    return this.hook.start({ enableClipboard: true });
  }

  stop(): boolean {
    return this.hook.stop();
  }

  cleanup(): void {
    this.hook.cleanup();
  }

  private handleSelection(data: TextSelectionData): void {
    this.callbacks.onSelection({
      text: data.text,
      programName: data.programName,
      method: data.method,
      methodName: describeMethod(data.method),
      endBottom: data.endBottom,
      mousePosEnd: data.mousePosEnd,
    });
  }

  private handleKeyDown(data: KeyboardEventData): void {
    this.callbacks.onKeyDown({
      key: data.uniKey,
      systemModifier: data.sys,
      flags: data.flags,
    });
  }
}

function describeMethod(method: TextSelectionData['method']): string {
  const names: Record<number, string> = {
    [SelectionHook.SelectionMethod.NONE]: 'None',
    [SelectionHook.SelectionMethod.UIA]: 'UI Automation',
    [SelectionHook.SelectionMethod.ACCESSIBLE]: 'IAccessible',
    [SelectionHook.SelectionMethod.AXAPI]: 'Accessibility API',
    [SelectionHook.SelectionMethod.ATSPI]: 'AT-SPI',
    [SelectionHook.SelectionMethod.PRIMARY]: 'Primary selection',
    [SelectionHook.SelectionMethod.CLIPBOARD]: 'Clipboard',
  };

  return names[method] ?? `Unknown (${method})`;
}
