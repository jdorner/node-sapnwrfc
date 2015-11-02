var os = require('os');
var child_process = require('child_process');
var red = '\u001b[31m',
  green = '\u001b[32m',
  reset = '\u001b[0m';

var nodeGyp = function () {
  switch (os.platform()) {
    case 'win32':
      return 'node-gyp.cmd';
    case 'linux':
    case 'darwin':
    default:
      return 'node-gyp';
  }
};

var rebuild = function () {
  if (os.platform() === 'darwin') {
    return console.log('NOT SUPPORTED UNDER DARWIN.');
  };

  try {
    var majMinVersion = process.versions.node.match(/^[0-9]+.[0-9]+/)[0] || '';
    var bindings = require('bindings')({ bindings: 'sapnwrfc', version: majMinVersion });
    console.log(green + 'ok ' + reset + 'found precompiled module at ' + bindings.path);
  } catch (e) {
    console.log(e);
    console.log(red + 'error ' + reset + 'a precompiled module could not be found or loaded');

    // Spawn gyp
    console.log(green + 'info ' + reset + 'trying to compile it...');
    var opts = {};
    opts.stdio = [0, 1, 2];
    opts.env = process.env;
    child_process.spawn(nodeGyp(), ['rebuild'], opts);
  }
};

var opts = {};
opts.stdio = [0, 1, 2];
var cp = child_process.spawn(nodeGyp(), ['clean'], opts);
cp.on('exit', rebuild);
