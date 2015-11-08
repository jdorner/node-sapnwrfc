/* global describe, before, it */
var mocha = require('mocha');
var should = require('should');
var sapnwrfc = require('../sapnwrfc');

describe('SetIniPath [ci]', function () {
  var con = undefined;

  before(function() {
    con = new sapnwrfc.Connection;
  });

  it('should work with current dir', function () {
    var rc = con.SetIniPath('.');
    rc.should.be.true();
  });

  it('should work with parent dir', function () {
    var rc = con.SetIniPath('..');
    rc.should.be.true();
  });

  it('should not throw an exception with non existing dir', function () {
    (function () {
      con.SetIniPath('/this/directory/does/not/exist');
    }).should.not.throw();
  });
});
