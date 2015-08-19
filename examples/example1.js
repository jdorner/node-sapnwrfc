"use strict";

var http = require('http');
var sapnwrfc = require('../sapnwrfc');

var connectionParams = {
  ashost: "192.168.0.20",
  sysid: "NPL",
  sysnr: "42",
  user: "DEVELOPER",
  passwd: "",
  client: "001"
};

var funcParams = {
  QUESTION: 'How are you'
}

http.createServer(function (req, res) {

  res.writeHead(200, {'Content-Type': 'text/plain'});

  var con = new sapnwrfc.Connection;
  con.Open(connectionParams, function(err) {
    if (err) {
      res.end(err.message);
      return;
    }
    res.write("Version: " + con.GetVersion() + "\n");

    var func = con.Lookup('STFC_STRING');
    func.Invoke(funcParams, function(err, result) {
      if (err) {
        res.end(err.message);
        return;
      }
      console.log(result);
      res.write(result.MYANSWER + "\n");
      res.write("\nJSON Schema:\n");
      res.end(JSON.stringify(func.MetaData(), null, 2));
    });
  });
}).listen(3000, '127.0.0.1');

console.log('Server running at http://127.0.0.1:3000/');