const addon = require('../native/win32/build/Release/selection_forward_win32.node');

const expectedExports = [
  'hideIndicator',
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
