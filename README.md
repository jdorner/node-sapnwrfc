# SAP Netweaver RFC SDK Bindings for Node.js

[![Build status](https://ci.appveyor.com/api/projects/status/oi489ecnryrt4pdg/branch/master?svg=true)](https://ci.appveyor.com/project/jdorner/node-sapnwrfc/branch/master) [![Circle CI](https://circleci.com/gh/jdorner/node-sapnwrfc/tree/master.svg?style=svg)](https://circleci.com/gh/jdorner/node-sapnwrfc/tree/master) [![npm version](https://badge.fury.io/js/sapnwrfc.svg)](https://badge.fury.io/js/sapnwrfc)

## Description

This module provides bindings to the SAP Netweaver RFC SDK. With it, you will be able to call remote enabled function modules of a SAP system.
Opening the connection and function invocations are fully/partially asynchronous operations, which will call a callback function upon completion.

## Preparation

**Note:** In order to use this module you will have to obtain the SAP NW RFC SDK via http://service.sap.com. For further instructions see OSS note 1025361.

### Linux

- Extract the SDK archive with SAPCAR
- Copy the files from the lib and include folders to the corresponding system directories (/usr/local/Lib /usr/local/include)

```sh
./SAPCAR_3-20002089.EXE -xf NWRFC_8-20004549.SAR
cd nwrfcsdk
cp ./lib/* /usr/lib
cp ./include/* /usr/inlude
```

### Windows

- Extract the SDK archive with SAPCAR
- Copy the files from the lib folder to C:\Windows\system32
- Warning: If you are working with NodeJS 32-bit on a Windows 64-bit OS, you should copy the files to C:\Windows\SysWOW64 instead.

### OS X

OS X is **not supported** due to the fact that there is no suitable SAP RFC NW SDK available. The module will compile but fail when trying to lazy-load its dependencies.

## Installation (both Linux and Windows)

- You may now download the addon from the [npm registry](http://search.npmjs.org) and install it by using the `npm` command.

```sh
npm install sapnwrfc
```

## Usage

As with all other Node.js modules, you need to require it:

```js
var sapnwrfc = require('sapnwrfc');
```

## Connection parameters

Connection parameters have to be specified using a JavaScript object. For a complete list of possible parameters, refer to the
_SAP NetWeaver RFC SDK Guide_ which is available via [SAP Connectors](http://service.sap.com/connectors).

Example:

```js
var conParams = {
  ashost: '192.168.0.10',
  sysid: 'NPL',
  sysnr: '42',
  user: 'DEVELOPER',
  passwd: 'password',
  client: '001',
  lang: 'E'
};
```

## Set path to sapnwrfc.ini

Before you open a connection you can set the directory path to look for the `sapnwrfc.ini` file.

Example:

```js
var con = new sapnwrfc.Connection;
var iniPath = '/path/to/dir/with/inifile/in/it'

con.SetIniPath(iniPath);
```

## Opening the connection

Before you can invoke a remote function, you will have to open a connection to the SAP system.

```js
Connection.Open( connectionParameters, callback( errorObject ) );
```

- **connectionParameters:** JavaScript object containing the parameters used for connecting to a SAP system (see above)
- **callback:** A function to be executed after the connection has been attempted. In case of an error, an errorObject will be passed as an argument.

```js
versionInfo = Connection.GetVersion( );
```

- **versionInfo:** An Array containing major number, minor number and patch level of the NW RFC SDK

Example:

```js
var con = new sapnwrfc.Connection;

con.Open(conParams, function(err) {
  if (err) {
    console.log(err);
    return;
  }
  console.log('sapnwrfc: ' + con.GetVersion());
});
```

## Calling a remote function module

This is a two step process:

- You will first have to lookup the function module's definition, getting a Function object in return
- After a successful lookup, you may invoke the function and pass arguments to it

However, you can use the Function object subsequently multiple times for invocations, without having to do another lookup upfront.

```js
functionObject = Connection.Lookup( functionModuleName )
```

- **functionModuleName:** A string containing the name of the remote function module to be called
- **functionObject:** A JavaScript object (class name: Function) which represents an interface to invoke the function

```js
Function.Invoke( functionParameters, callback( errorObject, result ) )
```

- **functionParameters:** JavaScript object containing the parameters used for connecting to a SAP system (see above)
- **callback:** A function to be executed after the connection has been attempted. In case of an error, an errorObject will be passed as an argument. The result will be returned as a JavaScriptObject (see below for details)

For the sake of simplicity, the following example will neither pass arguments to the remote function nor receive a result:

```js
var func = con.Lookup('RFC_PING');
func.Invoke({ }, function(err, result) {
  console.log('Got pong!');
});
```

## Passing and receiving arguments

Remote function arguments are being passed by using a plain JavaScript object. For each parameter to pass in, you'll have to define a
member property named according to the remote function's interface. There is no need to distinguish between importing, exporting or table
parameters.

### Primitives

Assigning primitive types (non-structures, non-tables) is straightforward. You will, however, have to take care that the argument matches
the parameter's data type. E.g. if the parameter type is an integer, you may not assign a string value to it.

Example:

```js
var params = {
  QUESTION: 'How are you'
};

var func = con.Lookup('STFC_STRING');
func.Invoke(params, function(err, result) {
  if (err) {
    console.log(err);
    return;
  }

  console.log(result);
});
```

### Binary data

SAP data types like XSTRING and RAW need some special treatment as JavaScript does not support binary data very well. In order to safely pass
binary data back and forth, you will have to use [Node Buffers](https://nodejs.org/api/buffer.html). This, of course, also holds true for binary
data types used within structures and tables.

Example:

```js

var func = con.Lookup('STFC_XSTRING');
var params = { QUESTION: new Buffer('00FF65', 'hex') };

func.Invoke(params, function (err, result) {
  console.log(result); // => <Buffer de ad>
});

```

### Structures

Structures are represented by JavaScriptObjects, where each field corresponds to a member property.

Example:

```js
var params = {
  IMPORTSTRUCT: { RFCFLOAT: 3.14159, RFCINT1: 123, RFCTIME: '094500', RFCCHAR4: 'NODE' }
};

var func = con.Lookup('STFC_STRUCTURE');
func.Invoke(params, function(err, result) {
  if (err) {
    console.log(err);
    return;
  }

  console.log(result);
});
```

### Tables

A table is nothing else than an array of structures. This means, in terms of JavaScript, that you need to put JavaScript objects into an array.

Example:

```js
var table = [
  { I: 1, C: 'A', STR: 'String1' },
  { I: 2, C: 'B', STR: 'String2' },
  { I: 3, C: 'C', STR: 'String3' }
];

var params = {
  IMPORT_TAB: table
};

var func = con.Lookup('STFC_DEEP_TABLE');
func.Invoke(params, function(err, result) {
  if (err) {
    console.log(err);
    return;
  }

  for (var i = 0; i < result.EXPORT_TAB.length; i++) {
    console.log('Row ' + (i + 1) + ':');
    console.log(result.EXPORT_TAB[i]);
  }
  console.log(result.RESPTEXT);
});
```

## Retrieving function signature as JSON Schema

You can retrieve the name and types of remote function arguments with MetaData() call.

Example:

```js
var func = con.Lookup('STFC_STRING');
var signature = func.MetaData();
console.log(JSON.stringify(signature, null, 2));
```

The console output is:
```js
{
  "title": "Signature of SAP RFC function STFC_STRING",
  "type": "object",
  "properties": {
    "MYANSWER": {
      "type": "string",
      "length": "0",
      "sapType": "RFCTYPE_STRING",
      "description": "",
      "sapDirection": "RFC_EXPORT"
    },
    "QUESTION": {
      "type": "string",
      "length": "0",
      "sapType": "RFCTYPE_STRING",
      "description": "",
      "sapDirection": "RFC_IMPORT"
    }
  }
}
```

The result of function MetaData() is an object of [JSON Schema](http://json-schema.org/latest/json-schema-core.html).

The *properties* sub-object specifies the parameter of the remote function. In the above example the remote function STFC_STRING has the parameters MYANSWER and QUESTION. The *sapDirection* specifies if it is an input parameter (RFC_IMPORT) or output parameter (RFC_EXPORT) or input and/or output (RFC_CHANGING | RFC_TABLES).

Attributes with the prefix *sap* are specific to this JSON Schema instance.

- **title:** Name of the JSON Schema.
- **type:** JavaScript type.
- **length:** Length of a simple type or structure.
- **description:** Description of parameters from SAP. Can be empty.
- **sapType:** Native SAP type. RFCTYPE_TABLE | RFCTYPE_STRUCTURE | RFCTYPE_STRING | RFCTYPE_INT | RFCTYPE_BCD | RFCTYPE_FLOAT | RFCTYPE_CHAR | RFCTYPE_DATE | RFCTYPE_TIME | RFCTYPE_BYTE | RFCTYPE_NUM | ... . You find the complete list of possible values in the SAP header file sapnwrfc.h. Look for enum type *RFCTYPE*.
- **sapDirection:** Attribute of the first level of properties. RFC_IMPORT | RFC_EXPORT | RFC_CHANGING | RFC_TABLES
- **sapTypeName:** Name of a structure or name of a structure of a table.


## Contributors
- Alfred Gebert
- Stefan Scherer
- Michael Scherer
- Szilard Novaki
- Jakub Zakrzewski
- Alex
- LeandroDG

## Changelog

### 0.2.0 (2015-11-11)
- Add SetIniPath
- RAW fields and XSTRINGs return/expect node Buffers

### 0.1.8 (2015-11-02)
- Automatic builds for Linux and Windows
- Add tests
- Bugfixes

### 0.1.7 (2015-11-01)
- Add gulp script for bulk compiling

### 0.1.6 (2015-11-01)
- Support for Node.js 4.x (and io.js)
- Fix library path issue (Windows)
- Add parameter msvs_nwrfcsdk_path for node-gyp
- Add MetaData()

### 0.1.5 (2013-05-25)
- Support for Node.js 0.10

### 0.1.4 (2013-02-09)
- Fix compilation on Linux (issue #2)

### 0.1.3 (2012-08-16)

- Support for Node.js >= 0.7.9
- Change global invocation lock to a connection based lock

## TODO

- Unit tests
- Missing but probably useful functions:
  - RfcIsConnectionHandleValid (aka Connection::IsOpen())
  - RfcRemoveFunctionDesc (invalidate cache)
  - RfcGetPartnerSSOTicket
- Use of buffers for xstring
- Event emission on disconnect

## License

(The MIT License)

Copyright (c) 2011-2012 Joachim Dorner

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
'Software'), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
