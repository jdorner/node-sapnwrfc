/*
-----------------------------------------------------------------------------
Copyright (c) 2011 Joachim Dorner

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
-----------------------------------------------------------------------------
*/

#ifndef COMMON_H_
#define COMMON_H_

#include <v8.h>
#include <nan.h>
#include <sapnwrfc.h>
#include <iostream>

#ifndef nullptr
#define nullptr NULL
#endif

#define ESCAPE_RFC_ERROR(...) scope.Escape(RfcError(__VA_ARGS__));
#define RETURN_RFC_ERROR(...) info.GetReturnValue().Set(RfcError(__VA_ARGS__)); return;

typedef DATA_CONTAINER_HANDLE CHND;

static std::string convertToString(v8::Local<v8::Value> const &str)
{
  Nan::HandleScope scope;
  static const std::string emptyString;

  v8::String::Utf8Value utf8String(str);
  const char *s = *utf8String;
  if (s != nullptr) {
    return std::string(s, utf8String.length());
  }

  return emptyString;
}

static std::string convertToString(const SAP_UC *str)
{
  Nan::HandleScope scope;
  v8::Local<v8::String> utf16String = Nan::New<v8::String>((const uint16_t*)(str)).ToLocalChecked();

  return convertToString(utf16String);
}

static SAP_UC* convertToSAPUC(v8::Local<v8::Value> const &str) {
  RFC_RC rc;
  RFC_ERROR_INFO errorInfo;
  const char *cStr;
  SAP_UC *sapuc;
  unsigned sapucSize, resultLength;

  std::string s = convertToString(str);
  cStr = s.c_str();

  sapucSize = strlen(cStr) + 1;
  sapuc = mallocU(sapucSize);
  memsetU(sapuc, 0, sapucSize);
  resultLength = 0;
  rc = RfcUTF8ToSAPUC((RFC_BYTE*)cStr, strlen(cStr), sapuc, &sapucSize, &resultLength, &errorInfo);

  return sapuc;
}

static v8::Local<v8::Value> RfcError(const RFC_ERROR_INFO &info)
{
  Nan::EscapableHandleScope scope;

  v8::Local<v8::Value> e = v8::Exception::Error(
      Nan::New<v8::String>((const uint16_t*)(info.message)).ToLocalChecked()
  );

  v8::Local<v8::Object> obj = e->ToObject();
  obj->Set(
      Nan::New<v8::String>("code").ToLocalChecked(),
      Nan::New<v8::Integer>(info.code)
  );
  obj->Set(
      Nan::New<v8::String>("group").ToLocalChecked(),
      Nan::New<v8::Integer>(info.group)
  );
  obj->Set(
      Nan::New<v8::String>("key").ToLocalChecked(),
      Nan::New<v8::String>((const uint16_t*)(info.key)).ToLocalChecked()
  );
  obj->Set(
      Nan::New<v8::String>("class").ToLocalChecked(),
      Nan::New<v8::String>((const uint16_t*)(info.abapMsgClass)).ToLocalChecked()
  );
  obj->Set(
      Nan::New<v8::String>("type").ToLocalChecked(),
      Nan::New<v8::String>((const uint16_t*)(info.abapMsgType)).ToLocalChecked()
  );
  obj->Set(
      Nan::New<v8::String>("number").ToLocalChecked(),
      Nan::New<v8::String>((const uint16_t*)(info.abapMsgNumber)).ToLocalChecked()
  );
  obj->Set(
      Nan::New<v8::String>("msgv1").ToLocalChecked(),
      Nan::New<v8::String>((const uint16_t*)(info.abapMsgV1)).ToLocalChecked()
  );
  obj->Set(
      Nan::New<v8::String>("msgv2").ToLocalChecked(),
      Nan::New<v8::String>((const uint16_t*)(info.abapMsgV2)).ToLocalChecked()
  );
  obj->Set(
      Nan::New<v8::String>("msgv3").ToLocalChecked(),
      Nan::New<v8::String>((const uint16_t*)(info.abapMsgV3)).ToLocalChecked()
  );
  obj->Set(
      Nan::New<v8::String>("msgv4").ToLocalChecked(),
      Nan::New<v8::String>((const uint16_t*)(info.abapMsgV4)).ToLocalChecked()
  );

  return scope.Escape(obj);
}

static v8::Local<v8::Value> RfcError(const char* message, v8::Local<v8::Value> value)
{
  Nan::EscapableHandleScope scope;

  v8::Local<v8::String> leftSide = Nan::New<v8::String>(message).ToLocalChecked();
  v8::Local<v8::String> exceptionString = v8::String::Concat(leftSide, value->ToString());

  return scope.Escape(Nan::Error(exceptionString));
}

static v8::Local<v8::Value> RfcError(const char *message, const SAP_UC *sapName) {
  Nan::EscapableHandleScope scope;
  v8::Local<v8::String> name = Nan::New<v8::String>((const uint16_t*)sapName).ToLocalChecked();

  return scope.Escape(RfcError(message, name));
}

static bool IsException(const v8::Local<v8::Value> &value)
{
  return value->IsNativeError();
}

#endif /* COMMON_H_ */
