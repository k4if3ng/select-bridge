import { basename } from 'node:path';

import { isTriggerMode, type AppConfig, type TriggerMode } from '../config.js';
import type { PlatformEvent, PlatformHost } from '../platform/types.js';
import type { KeyEvent, ScreenPoint, SelectionEvent } from '../selection/types.js';
import type { TranslationTarget } from '../targets/types.js';

interface Candidate {
  id: number;
  text: string;
  source: SelectionEvent;
  position?: ScreenPoint;
  ready: boolean;
}

export interface TriggerControllerOptions {
  config: AppConfig;
  platform: PlatformHost;
  target: TranslationTarget;
  onConfigChange(config: AppConfig): void;
  onExitRequested(): void;
}

export class TriggerController {
  private config: AppConfig;
  private candidate?: Candidate;
  private candidateSequence = 0;
  private stableTimer?: NodeJS.Timeout;
  private expiryTimer?: NodeJS.Timeout;
  private lastForwardedText = '';
  private lastForwardedProgram = '';
  private lastForwardedAt = 0;

  constructor(private readonly options: TriggerControllerOptions) {
    this.config = options.config;
  }

  handleSelection(event: SelectionEvent): void {
    this.cancelCandidate();

    if (!this.config.enabled) {
      return;
    }

    const text = normalizeSelection(event.text);
    if (!text || isGoldenDict(event.programName)) {
      return;
    }

    if (text.length > this.config.maxTextLength) {
      console.warn(
        `[skip] 选区长度为 ${text.length}，超过限制 ${this.config.maxTextLength}。`,
      );
      return;
    }

    const candidate: Candidate = {
      id: ++this.candidateSequence,
      text,
      source: event,
      position: resolvePosition(event),
      ready: false,
    };

    this.candidate = candidate;
    this.stableTimer = setTimeout(() => this.settleCandidate(candidate.id), this.config.selectionStableMs);
  }

  handleKeyDown(event: KeyEvent): void {
    const expectedKey = getModifierKey(this.config.triggerMode);
    if (!expectedKey || event.key.toLowerCase() !== expectedKey.toLowerCase()) {
      return;
    }

    this.triggerCandidate('shortcut');
  }

  handlePlatformEvent(event: PlatformEvent): void {
    switch (event.type) {
      case 'indicator-click':
        if (this.config.indicatorAction === 'click') {
          this.triggerCandidate('click');
        }
        return;
      case 'indicator-hover':
        if (this.config.indicatorAction === 'hover') {
          this.triggerCandidate('hover');
        }
        return;
      case 'shortcut':
        if (this.config.triggerMode === 'custom') {
          this.triggerCandidate('shortcut');
        }
        return;
      case 'shortcut-conflict':
        console.error(`[shortcut] 快捷键注册冲突：${event.value}`);
        return;
      case 'native-error':
        console.error(`[platform] 原生窗口错误：${event.value}`);
        return;
      case 'toggle-enabled':
        this.replaceConfig({ ...this.config, enabled: !this.config.enabled });
        return;
      case 'set-trigger-mode':
        if (isTriggerMode(event.value)) {
          this.replaceConfig({ ...this.config, triggerMode: event.value });
        }
        return;
      case 'set-indicator-action':
        if (event.value === 'click' || event.value === 'hover') {
          this.replaceConfig({ ...this.config, indicatorAction: event.value });
        }
        return;
      case 'set-icon-size':
        this.replaceConfig({
          ...this.config,
          iconSize: clampInteger(event.value, this.config.iconSize, 32, 64),
        });
        return;
      case 'set-dot-size':
        this.replaceConfig({
          ...this.config,
          dotSize: clampInteger(event.value, this.config.dotSize, 12, 28),
        });
        return;
      case 'set-custom-shortcut':
        this.replaceConfig({ ...this.config, customShortcut: event.value });
        return;
      case 'open-settings':
        return;
      case 'exit':
        this.options.onExitRequested();
        return;
      case 'toggle-auto-start':
        return;
    }
  }

  replaceConfig(config: AppConfig): void {
    this.config = config;
    this.cancelCandidate();
    this.options.platform.updateState({
      enabled: config.enabled,
      triggerMode: config.triggerMode,
      autoStart: config.autoStart,
      indicatorAction: config.indicatorAction,
      iconSize: config.iconSize,
      dotSize: config.dotSize,
      customShortcut: config.customShortcut,
    });
    this.options.onConfigChange(config);
  }

  dispose(): void {
    this.cancelCandidate();
  }

  private settleCandidate(candidateId: number): void {
    const candidate = this.candidate;
    if (!candidate || candidate.id !== candidateId) {
      return;
    }

    candidate.ready = true;
    this.stableTimer = undefined;

    if (this.config.triggerMode === 'immediate') {
      this.triggerCandidate('immediate');
      return;
    }

    if (this.config.triggerMode === 'icon' || this.config.triggerMode === 'dot') {
      this.options.platform.showIndicator({
        x: candidate.position?.x,
        y: candidate.position?.y,
        style: this.config.triggerMode,
        size:
          this.config.triggerMode === 'dot' ? this.config.dotSize : this.config.iconSize,
        hoverEnabled: this.config.indicatorAction === 'hover',
        hoverDelayMs: this.config.hoverDelayMs,
      });
    }

    this.expiryTimer = setTimeout(() => this.cancelCandidate(), this.config.indicatorTtlMs);
  }

  private triggerCandidate(reason: TriggerReason): void {
    const candidate = this.candidate;
    if (!candidate?.ready) {
      return;
    }

    const now = Date.now();
    if (
      candidate.text === this.lastForwardedText &&
      candidate.source.programName === this.lastForwardedProgram &&
      now - this.lastForwardedAt < this.config.dedupeWindowMs
    ) {
      this.cancelCandidate();
      return;
    }

    this.lastForwardedText = candidate.text;
    this.lastForwardedProgram = candidate.source.programName;
    this.lastForwardedAt = now;
    this.candidate = undefined;
    this.clearCandidateTimers();
    this.options.platform.hideIndicator();

    console.log(
      `[lookup:${reason}] ${JSON.stringify(candidate.text)}` +
        `${candidate.source.programName ? `，来源：${candidate.source.programName}` : ''}` +
        `（检测方式：${candidate.source.methodName}）`,
    );

    void this.options.target.translate(candidate.text).catch((error: unknown) => {
      console.error('[lookup] Goldendict-ng 查询失败：', error);
    });
  }

  private cancelCandidate(): void {
    this.candidate = undefined;
    this.clearCandidateTimers();
    this.options.platform.hideIndicator();
  }

  private clearCandidateTimers(): void {
    if (this.stableTimer) {
      clearTimeout(this.stableTimer);
      this.stableTimer = undefined;
    }
    if (this.expiryTimer) {
      clearTimeout(this.expiryTimer);
      this.expiryTimer = undefined;
    }
  }
}

type TriggerReason = 'immediate' | 'shortcut' | 'click' | 'hover';

function normalizeSelection(text: string): string {
  return text.replace(/\s+/g, ' ').trim();
}

function isGoldenDict(programName: string): boolean {
  return /goldendict/i.test(basename(programName));
}

function resolvePosition(event: SelectionEvent): ScreenPoint | undefined {
  if (isValidPoint(event.endBottom)) {
    return event.endBottom;
  }
  if (isValidPoint(event.mousePosEnd)) {
    return event.mousePosEnd;
  }
  return undefined;
}

function isValidPoint(point: ScreenPoint): boolean {
  return Number.isFinite(point.x) && Number.isFinite(point.y) && point.x !== -99999 && point.y !== -99999;
}

function getModifierKey(mode: TriggerMode): string | undefined {
  return ({ ctrl: 'Control', alt: 'Alt', shift: 'Shift' } as Partial<Record<TriggerMode, string>>)[mode];
}

function clampInteger(
  value: string,
  fallback: number,
  minimum: number,
  maximum: number,
): number {
  const parsed = Number.parseInt(value, 10);
  return Number.isSafeInteger(parsed)
    ? Math.min(maximum, Math.max(minimum, parsed))
    : fallback;
}
