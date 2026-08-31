type ModifierKey = 'Control' | 'Alt' | 'Shift' | 'Meta';

interface ParsedShortcut {
  modifiers: ReadonlySet<ModifierKey>;
  key: string;
}

const MODIFIER_ORDER: readonly ModifierKey[] = ['Control', 'Alt', 'Shift', 'Meta'];

export class PortableShortcutMatcher {
  private readonly pressedModifiers = new Set<ModifierKey>();

  keyDown(key: string, shortcut: string): boolean {
    const modifier = normalizeModifier(key);
    if (modifier) {
      this.pressedModifiers.add(modifier);
      return false;
    }

    const parsed = parsePortableShortcut(shortcut);
    if (!parsed || normalizePrimaryKey(key) !== parsed.key) {
      return false;
    }

    return modifiersEqual(this.pressedModifiers, parsed.modifiers);
  }

  keyUp(key: string): void {
    const modifier = normalizeModifier(key);
    if (modifier) {
      this.pressedModifiers.delete(modifier);
    }
  }

  reset(): void {
    this.pressedModifiers.clear();
  }
}

export function isPortableShortcut(shortcut: string): boolean {
  return parsePortableShortcut(shortcut) !== undefined;
}

function parsePortableShortcut(shortcut: string): ParsedShortcut | undefined {
  const tokens = shortcut
    .split('+')
    .map((token) => token.trim());
  if (tokens.length < 2 || tokens.some((token) => !token)) {
    return undefined;
  }

  const modifiers = new Set<ModifierKey>();
  let primaryKey: string | undefined;
  for (const token of tokens) {
    const modifier = normalizeModifier(token);
    if (modifier) {
      modifiers.add(modifier);
      continue;
    }

    const key = normalizePrimaryKey(token);
    if (!key || primaryKey) {
      return undefined;
    }
    primaryKey = key;
  }

  return modifiers.size > 0 && primaryKey
    ? { modifiers, key: primaryKey }
    : undefined;
}

function normalizeModifier(key: string): ModifierKey | undefined {
  switch (key.trim().toLowerCase()) {
    case 'ctrl':
    case 'control':
      return 'Control';
    case 'alt':
    case 'option':
      return 'Alt';
    case 'shift':
      return 'Shift';
    case 'win':
    case 'windows':
    case 'lwin':
    case 'rwin':
    case 'meta':
    case 'super':
    case 'command':
    case 'cmd':
    case 'os':
      return 'Meta';
    default:
      return undefined;
  }
}

function normalizePrimaryKey(key: string): string | undefined {
  const trimmed = key.trim();
  if (!trimmed) {
    return key === ' ' ? 'Space' : undefined;
  }

  if (/^[a-z0-9]$/i.test(trimmed)) {
    return trimmed.toUpperCase();
  }

  const functionKey = /^f([1-9]|1\d|2[0-4])$/i.exec(trimmed);
  if (functionKey) {
    return `F${functionKey[1]}`;
  }

  const aliases: Readonly<Record<string, string>> = {
    esc: 'Escape',
    escape: 'Escape',
    tab: 'Tab',
    space: 'Space',
    spacebar: 'Space',
    enter: 'Enter',
    return: 'Enter',
    backspace: 'Backspace',
    insert: 'Insert',
    delete: 'Delete',
    del: 'Delete',
    home: 'Home',
    end: 'End',
    pageup: 'PageUp',
    pgup: 'PageUp',
    pagedown: 'PageDown',
    pgdn: 'PageDown',
    up: 'Up',
    arrowup: 'Up',
    down: 'Down',
    arrowdown: 'Down',
    left: 'Left',
    arrowleft: 'Left',
    right: 'Right',
    arrowright: 'Right',
    printscreen: 'PrintScreen',
    pause: 'Pause',
  };
  return aliases[trimmed.toLowerCase()];
}

function modifiersEqual(
  pressed: ReadonlySet<ModifierKey>,
  expected: ReadonlySet<ModifierKey>,
): boolean {
  return (
    pressed.size === expected.size &&
    MODIFIER_ORDER.every((modifier) => pressed.has(modifier) === expected.has(modifier))
  );
}
