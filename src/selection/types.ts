export interface ScreenPoint {
  x: number;
  y: number;
}

export interface SelectionEvent {
  text: string;
  programName: string;
  method: number;
  methodName: string;
  endBottom: ScreenPoint;
  mousePosEnd: ScreenPoint;
}

export interface KeyEvent {
  key: string;
  systemModifier: boolean;
  flags: number;
}
