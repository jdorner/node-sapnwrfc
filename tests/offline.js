/* global describe, before, it, context */
var mocha = require('mocha');
var should = require('should');
var sapnwrfc = require('../sapnwrfc');

var connectionParams = {
  ashost: "192.168.0.14",
  sysid: "NSP",
  sysnr: "00",
  user: "BCUSER",
  passwd: "ch4ngeme",
  client: "001"
};


describe('Offline tests [ci]', function () {

  var con = undefined;

  before(function () {
    con = new sapnwrfc.Connection;
  });

  context('Connection', function () {
    it('should be an instance', function () {
      con.should.be.an.Object();
    });

    it('should return a version number', function () {
      var version = con.GetVersion();
      version.should.be.an.Array().and.have.length(3);
    });

    it('should still be closed', function () {
      con.IsOpen().should.be.false();
    });
  });

  context('Closed connection', function () {
    it('should fail on ping', function () {
      var pong = con.Ping();
      pong.should.be.an.Error();
      should(pong.key).equal('RFC_INVALID_HANDLE');
    });
  });
});


describe('Online tests', function () {

  this.timeout(0);
  var con = undefined;

  before(function (done) {
    con = new sapnwrfc.Connection;
    con.Open(connectionParams, function (err) {
      should(err).be.Null();
      done();
    });
  });

  context('Open connection', function () {
    it('should be open', function () {
      con.IsOpen().should.be.true();
    });

    it('should pong my ping', function () {
      var pong = con.Ping();
      pong.should.be.true();
    });

    it('should fail on lookup of a non-existing FM', function () {
      var func = con.Lookup('AAAAAAAA');
      func.should.be.an.instanceof(Error).and.have.enumerable('key');
      should(func.key).equal('FU_NOT_FOUND');
    });

    it('should succeed on lookup of an existing FM', function () {
      var func = con.Lookup('RFC_PING');
      func.should.not.be.an.Error();
      func.Invoke.should.be.an.Function;
    });
  });

  context('Simple function calls', function () {
    it('should return Hello world!', function (done) {
      var func = con.Lookup('STFC_CONNECTION');
      var params = { REQUTEXT: 'Hello world!' };

      func.Invoke(params, function (err, result) {
        should(err).be.null;

        result.should.be.an.Object();
        result.should.have.property('ECHOTEXT').and.startWith('Hello world!');
        done();
      });
    });

    it('should speak Unicode', function (done) {
      var func = con.Lookup('STFC_CONNECTION');
      var params = { REQUTEXT: '你好世界' };

      func.Invoke(params, function (err, result) {
        should(err).be.Null();

        result.should.be.an.Object();
        result.should.have.property('ECHOTEXT').and.startWith('你好世界');
        done();
      });
    });

    it('should return a structure', function (done) {
      var func = con.Lookup('STFC_STRUCTURE');
      var params = {
        IMPORTSTRUCT: {
          RFCCHAR1: 'A',
          RFCCHAR2: 'BC',
          RFCCHAR4: 'NODE',
          RFCDATA1: 'DEFGHIJKLMNOPQRSTUVWXYZ                          $',
          RFCDATA2: '^                                       1234567890',
          RFCDATE: '20151231',
          RFCFLOAT: 3.14159,
          RFCHEX3: new Buffer('C0FFEE', 'hex'),
          RFCINT1: 1,
          RFCINT2: 12,
          RFCINT4: 1234,
          RFCTIME: '094500'
        }
      };
      params.RFCTABLE = [{}, params.IMPORTSTRUCT];

      func.Invoke(params, function (err, result) {
        should(err).be.Null();

        result.should.be.an.Object();
        result.should.have.property('IMPORTSTRUCT').and.be.an.Object();
        result.should.have.property('ECHOSTRUCT').and.be.an.Object();
        result.ECHOSTRUCT.should.eql(params.IMPORTSTRUCT);

        result.should.have.property('RFCTABLE').and.be.an.Array();
        result.RFCTABLE.should.have.length(3);

        // Row 1
        result.RFCTABLE[0].should.be.an.Object();
        result.RFCTABLE[0].RFCCHAR1.should.equal(' ');
        result.RFCTABLE[0].RFCFLOAT.should.equal(0);
        result.RFCTABLE[0].RFCDATE.should.equal('00000000');
        result.RFCTABLE[0].RFCTIME.should.equal('000000');
        result.RFCTABLE[0].RFCHEX3.should.eql(new Buffer('000000', 'hex'));

        // Row 2
        result.RFCTABLE[1].should.be.an.Object();
        result.RFCTABLE[1].should.eql(params.IMPORTSTRUCT);

        // Row 3
        result.RFCTABLE[2].should.be.an.Object();
        result.RFCTABLE[2].RFCCHAR1.should.equal('X');
        result.RFCTABLE[2].RFCCHAR2.should.equal('YZ');
        result.RFCTABLE[2].RFCCHAR4.should.startWith(connectionParams.sysid);
        result.RFCTABLE[2].RFCINT1.should.equal(params.IMPORTSTRUCT.RFCINT1 + 1);
        result.RFCTABLE[2].RFCINT2.should.equal(params.IMPORTSTRUCT.RFCINT2 + 1);
        result.RFCTABLE[2].RFCINT4.should.equal(params.IMPORTSTRUCT.RFCINT4 + 1);
        result.RFCTABLE[2].RFCFLOAT.should.equal(params.IMPORTSTRUCT.RFCFLOAT + 1);
        result.RFCTABLE[2].RFCHEX3.should.eql(new Buffer('F1F2F3', 'hex'));

        done();
      });
    });

    it('should handle XSTRING parameters', function (done) {
      var func = con.Lookup('STFC_XSTRING');
      var params = { QUESTION: new Buffer('C0FFEE', 'hex') };

      func.Invoke(params, function (err, result) {
        should(err).be.Null();

        result.should.be.an.Object();
        result.should.have.property('QUESTION');
        result.QUESTION.should.eql(new Buffer('C0FFEE', 'hex'));
        result.should.have.property('MYANSWER');
        result.MYANSWER.should.eql(new Buffer('DEAD', 'hex'));
        done();
      });
    });

    it('should only allow Buffers for XSTRING parameters', function (done) {
      var func = con.Lookup('STFC_XSTRING');
      var params = { QUESTION: 'test' };

      func.Invoke(params, function (err, result) {
        err.should.be.an.Error().and.have.property('message');
        err.message.should.match(/.*?unexpected type.*$/);
        done();
      });
    });

    it('should support changing parameters', function (done) {
      var func = con.Lookup('STFC_CHANGING');
      var params = { START_VALUE: 100, COUNTER: 5 };

      func.Invoke(params, function (err, result) {
        should(err).be.Null();

        result.should.be.an.Object();
        result.should.have.property('START_VALUE');
        result.RESULT.should.equal(105);
        result.COUNTER.should.equal(6);
        done();
      });
    });

    it('should return an exception', function (done) {
      var func = con.Lookup('STFC_EXCEPTION');
      var params = {};

      func.Invoke(params, function (err, result) {
        should(err).not.be.Null();
        err.should.have.property('type').and.equal('E');
        err.should.have.property('key').and.equal('EXAMPLE');
        done();
      });
    });
  });

  /*context('Load tests', function () {
    it('should not run out of memory 1', function (done) {
      for (var i = 0; i < 10000; i++) {
        var func = con.Lookup('BAPI_FLIGHT_GETLIST');
      }
      done();
    });

    it('should not run out of memory 2', function (done) {
      var num_loops = 100000;
      var params = {};
      var j = 1;

      var func = con.Lookup('BAPI_FLIGHT_GETLIST');

      var handler = function (err, result) {
        if (err) {
          console.log(err);
        }
        if (j >= num_loops) {
          console.log("done: ", j);
          done();
        }
        j++;
      };

      for (var i = 1; i <= num_loops; i++) {
        if (i >= num_loops) {
          console.log("started: ", i);
        }
        func.Invoke(params, handler);
      }
    });
  });*/
});