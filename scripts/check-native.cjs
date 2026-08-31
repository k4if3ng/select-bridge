const addon = require('../native/win32/build/Release/select_bridge_win32_ui.node');

const expectedExports = [
  'hideIndicator',
  'openExternalUrl',
  'registerShortcut',
  'setAutoStart',
  'showIndicator',
  'start',
  'stop',
  'updateTray',
];

const missingExports = expectedExports.filter((name) => typeof addon[name] !== 'function');
if (missingExports.length > 0) {
  throw new Error(
    `Missing native functions: ${missingExports.join(', ')}`,
  );
}

console.log(`native-addon-load-ok: ${expectedExports.join(', ')}`);
