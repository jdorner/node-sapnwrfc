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
#include <sapnwrfc.h>
#include <iostream>

#ifndef _WIN32
#define USE_PTHREADS
#endif

#ifndef nullptr
#define nullptr NULL
#endif

#define THROW_V8_EXCEPTION(msg) ThrowException(v8::Exception::Error(v8::String::New(msg)));
#define RFC_ERROR(...) scope.Close(RfcError(__VA_ARGS__))

static std::string convertToString(v8::Handle<v8::Value> const &str)
{
  v8::HandleScope scope;
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
  v8::HandleScope scope;
  v8::Local<v8::String> utf16String = v8::String::New((const uint16_t*)(str));
  
  return convertToString(utf16String);
}

static SAP_UC* convertToSAPUC(v8::Handle<v8::Value> const &str) {
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

static v8::Handle<v8::Value> RfcError(const RFC_ERROR_INFO &info)
{
  v8::HandleScope scope;
  
  v8::Local<v8::Value> e = v8::Exception::Error(v8::String::New((const uint16_t*)(info.message)));
  
  v8::Local<v8::Object> obj = e->ToObject();
  obj->Set(v8::String::New("code"), v8::Integer::New(info.code));
  obj->Set(v8::String::New("group"), v8::Integer::New(info.group));
  obj->Set(v8::String::New("key"), v8::String::New((const uint16_t*)(info.key)));
  obj->Set(v8::String::New("class"), v8::String::New((const uint16_t*)(info.abapMsgClass)));
  obj->Set(v8::String::New("type"), v8::String::New((const uint16_t*)(info.abapMsgType)));
  obj->Set(v8::String::New("number"), v8::String::New((const uint16_t*)(info.abapMsgNumber)));
  obj->Set(v8::String::New("msgv1"), v8::String::New((const uint16_t*)(info.abapMsgV1)));
  obj->Set(v8::String::New("msgv2"), v8::String::New((const uint16_t*)(info.abapMsgV2)));
  obj->Set(v8::String::New("msgv3"), v8::String::New((const uint16_t*)(info.abapMsgV3)));
  obj->Set(v8::String::New("msgv4"), v8::String::New((const uint16_t*)(info.abapMsgV4)));
  
  return scope.Close(obj);
}

static v8::Handle<v8::Value> RfcError(const char* message, v8::Handle<v8::Value> value)
{
  v8::HandleScope scope;
  
  v8::Local<v8::String> exceptionString = v8::String::Concat(v8::String::New(message), value->ToString());
  v8::Local<v8::Value> e = v8::Exception::Error(exceptionString);

  return scope.Close(e->ToObject());
}

static bool IsException(v8::Handle<v8::Value> value)
{
  v8::HandleScope scope;
  const v8::Local<v8::Value> sample = v8::Exception::Error(v8::String::New(""));
  const v8::Local<v8::String> protoSample = sample->ToObject()->ObjectProtoToString();
  
  if (!value->IsObject()) {
    return false;
  }
  v8::Local<v8::String> protoReal = value->ToObject()->ObjectProtoToString();
  if (protoReal->Equals(protoSample)) {
    return true;
  }
  scope.Close(v8::Undefined());

  return false;
}

#endif /* COMMON_H_ */
