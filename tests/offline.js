/* global describe, before, it */
var mocha = require('mocha');
var should = require('should');
var sapnwrfc = require('../sapnwrfc');

describe('Connection (offline)', function () {
  var con = undefined;

  before(function() {
    con = new sapnwrfc.Connection;
  });

  it('should be an instance', function () {
    con.should.be.an.Object();
  });

  it('should return a version number', function () {
    var version = con.GetVersion();
    version.should.be.an.Array();
    version.should.have.length(3);
  });

  it('should still be closed', function () {
    con.IsOpen().should.be.false();
  });
});