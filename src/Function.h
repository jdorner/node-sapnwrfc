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

#ifndef FUNCTION_H_
#define FUNCTION_H_

#include "Common.h"
#include <node.h>
#include <v8.h>
#include <node_version.h>
#include <sapnwrfc.h>
#include "Connection.h"

class Function : public node::ObjectWrap
{
  public:
  static NAN_MODULE_INIT(Init);
  static v8::Local<v8::Value> NewInstance(Connection &connection, const Nan::NAN_METHOD_ARGS_TYPE args);

  protected:
  Function();
  ~Function();

  static NAN_METHOD(New);
  static NAN_METHOD(Invoke);
  static NAN_METHOD(MetaData);

  static void EIO_Invoke(uv_work_t *req);
  static void EIO_AfterInvoke(uv_work_t *req);

  v8::Local<v8::Value> DoReceive(const CHND container);

  v8::Local<v8::Value> SetValue(const CHND container, RFCTYPE type, const SAP_UC *name, unsigned len, v8::Local<v8::Value> value);
  v8::Local<v8::Value> StructureToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value);
  v8::Local<v8::Value> StructureToExternal(const CHND container, const RFC_STRUCTURE_HANDLE struc, v8::Local<v8::Value>);
  v8::Local<v8::Value> TableToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value);
  v8::Local<v8::Value> StringToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value);
  v8::Local<v8::Value> XStringToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value);
  v8::Local<v8::Value> NumToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value, unsigned len);
  v8::Local<v8::Value> CharToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value, unsigned len);
  v8::Local<v8::Value> ByteToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value, unsigned len);
  v8::Local<v8::Value> IntToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value);
  v8::Local<v8::Value> Int1ToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value);
  v8::Local<v8::Value> Int2ToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value);
  v8::Local<v8::Value> FloatToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value);
  v8::Local<v8::Value> TimeToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value);
  v8::Local<v8::Value> DateToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value);
  v8::Local<v8::Value> BCDToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value);

  v8::Local<v8::Value> GetValue(const CHND container, RFCTYPE type, const SAP_UC *name, unsigned len);
  v8::Local<v8::Value> StructureToInternal(const CHND container, const SAP_UC *name);
  v8::Local<v8::Value> StructureToInternal(const CHND container, const RFC_STRUCTURE_HANDLE struc);
  v8::Local<v8::Value> TableToInternal(const CHND container, const SAP_UC *name);
  v8::Local<v8::Value> StringToInternal(const CHND container, const SAP_UC *name);
  v8::Local<v8::Value> XStringToInternal(const CHND container, const SAP_UC *name);
  v8::Local<v8::Value> NumToInternal(const CHND container, const SAP_UC *name, unsigned len);
  v8::Local<v8::Value> CharToInternal(const CHND container, const SAP_UC *name, unsigned len);
  v8::Local<v8::Value> ByteToInternal(const CHND container, const SAP_UC *name, unsigned len);
  v8::Local<v8::Value> IntToInternal(const CHND container, const SAP_UC *name);
  v8::Local<v8::Value> Int1ToInternal(const CHND container, const SAP_UC *name);
  v8::Local<v8::Value> Int2ToInternal(const CHND container, const SAP_UC *name);
  v8::Local<v8::Value> FloatToInternal(const CHND container, const SAP_UC *name);
  v8::Local<v8::Value> DateToInternal(const CHND container, const SAP_UC *name);
  v8::Local<v8::Value> TimeToInternal(const CHND container, const SAP_UC *name);
  v8::Local<v8::Value> BCDToInternal(const CHND container, const SAP_UC *name);

  static std::string mapExternalTypeToJavaScriptType(RFCTYPE sapType);
  static bool addMetaData(const CHND container, v8::Local<v8::Object>& parent,
                          const RFC_ABAP_NAME name, RFCTYPE type,
                          unsigned int length, RFC_DIRECTION direction,
                          RFC_ERROR_INFO* errorInfo, RFC_PARAMETER_TEXT paramText = nullptr);

  class InvocationBaton
  {
    public:
    InvocationBaton() : function(nullptr), functionHandle(nullptr) { };
    ~InvocationBaton() {
      RFC_ERROR_INFO errorInfo;

      if (this->functionHandle) {
        RfcDestroyFunction(this->functionHandle, &errorInfo);
        this->functionHandle = nullptr;
      }

      if (this->function) {
        this->function->Unref();
      }

      delete this->cbInvoke;
      this->cbInvoke = nullptr;
    };

    Function *function;
    Connection *connection;
    RFC_FUNCTION_HANDLE functionHandle;
    Nan::Callback *cbInvoke;
    RFC_ERROR_INFO errorInfo;
  };

  static Nan::Persistent<v8::Function> ctor;

  Connection *connection;
  RFC_FUNCTION_DESC_HANDLE functionDescHandle;
};

#endif /* FUNCTION_H_ */
