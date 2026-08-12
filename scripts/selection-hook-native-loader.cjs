const { createRequire } = require('node:module');
const { dirname, join } = require('node:path');

const fileRequire = createRequire(__filename);

module.exports = function loadSelectionHookNativeAddon() {
  const nativePath = join(dirname(process.execPath), 'selection-hook.node');
  return fileRequire(nativePath);
};
