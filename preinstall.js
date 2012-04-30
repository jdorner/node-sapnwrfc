var red = '\u001b[31m',
    green = '\u001b[32m',
    reset = '\u001b[0m';

try {
  var bindings = require('bindings')('sapnwrfc');
  console.log(green + 'ok ' + reset + 'found precompiled module at ' + bindings.path);
} catch (e) {
  var os = require('os');
  var child_process = require('child_process');

  console.log(e);
  console.log(red + 'error ' + reset + 'a precompiled module could not be found or loaded');
  
  // Spawn gyp
  console.log(green + 'info ' + reset + 'trying to compile it...');
  var opts = {};
  opts.customFds = [ 0, 1, 2 ];
  opts.env = process.env;
  switch (os.platform()) {
    case 'win32':
      child_process.spawn('node-gyp.cmd', ['rebuild'], opts);
      break;
    case 'linux':
    case 'darwin':
      child_process.spawn('node-gyp', ['rebuild'], opts);
      break;
    default:
      console.log(red + 'error ' + reset + 'unknown platform: ' + os.platform());
      break;
  }
}