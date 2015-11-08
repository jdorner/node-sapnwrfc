var gulp = require('gulp');
var gutil = require('gulp-util');
var mocha = require('gulp-mocha');
var exec = require('child_process').exec;
var runSequence = require('run-sequence');
var path = require('path');

var nodeVersions = ['4.0.0', '4.1.2', '4.2.1', '5.0.0'];

var TaskBuilder = function (version) {
  var sourceFile = path.join('build', 'Release', 'sapnwrfc.node');
  var targetPathParts = {
    cwd: process.cwd(),
    compiled: process.env.NODE_BINDINGS_COMPILED_DIR || 'compiled',
    version: version.match(/^[0-9]+.[0-9]+/)[0] || '',
    platform: process.platform,
    arch: process.arch
  };

  var targetPathArray = [];
  for (var item in targetPathParts) {
    targetPathArray.push(targetPathParts[item]);
  }
  var targetPath = path.join.apply(null, targetPathArray);

  return function (cb) {
    var opts = {};
    opts.stdio = [0, 1, 2];
    opts.env = process.env;
    opts.maxBuffer = 500 * 1024;

    gutil.log('Building addon for', gutil.colors.cyan(version));
    exec('. $NVM_DIR/nvm.sh && nvm install ' + version + ' && npm install', opts, function (err, stdout, stderr) {
      console.log(stdout);
      if (err) {
        console.log(stderr);
        cb(err);
        return;
      }
      gulp.src(sourceFile).pipe(gulp.dest(targetPath));
      cb(err);
    });
  };
};

gulp.task('default', ['build']);

gulp.task('run', function (cb) {
  exec('export LD_LIBRARY_PATH=nwrfcsdk/lib; node examples/example2.js', function (err, stdout, stderr) {
    console.log(stdout);
    console.log(stderr);
  });
});

gulp.task('test', function (cb) {
  var mochaOptions = {};

  // Run only specific tests if we are on a CI server
  if (process.env.CI) {
    mochaOptions.grep = '.*?[\[]ci].*';
  }

  return gulp.src('tests/*', { read: false }).pipe(mocha(mochaOptions));
});

gulp.task('build', ['prepareBuild'], function (cb) {
  if (nodeVersions && nodeVersions.length) {
    runSequence.apply(null, nodeVersions);
  }
});

// Create a task for every version of node.js
gulp.task('prepareBuild', function (cb) {
  function registerVersionTasks(versions) {
    for (var i = 0, len = nodeVersions.length; i < len; i++) {
      var version = nodeVersions[i];
      gutil.log('Creating task', gutil.colors.cyan(version));
      gulp.task(version, TaskBuilder(version));
    }
  }

  registerVersionTasks(nodeVersions);
  cb();
});

