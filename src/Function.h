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

typedef DATA_CONTAINER_HANDLE CHND;

class Function : public node::ObjectWrap
{
  public:
  static void Init(v8::Handle<v8::Object> target);
  static v8::Handle<v8::Value> NewInstance(Connection &connection, const v8::Arguments &args);

  protected:
  Function();
  ~Function();

  static v8::Handle<v8::Value> New(const v8::Arguments &args);
  static v8::Handle<v8::Value> Invoke(const v8::Arguments &args);

  static void EIO_Invoke(uv_work_t *req);
  static void EIO_AfterInvoke(uv_work_t *req);

  v8::Handle<v8::Value> DoReceive(const CHND container, v8::Handle<v8::External> functionHandle);

  v8::Handle<v8::Value> SetParameter(const CHND container, RFC_PARAMETER_DESC &desc, v8::Handle<v8::Value> value);
  v8::Handle<v8::Value> SetField(const CHND container, RFC_FIELD_DESC &desc, v8::Handle<v8::Value> value);
  v8::Handle<v8::Value> SetValue(const CHND container, RFCTYPE type, const SAP_UC *name, unsigned len, v8::Handle<v8::Value> value);
  v8::Handle<v8::Value> StructureToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value);
  v8::Handle<v8::Value> StructureToExternal(const CHND container, const RFC_STRUCTURE_HANDLE struc, v8::Handle<v8::Value>);
  v8::Handle<v8::Value> TableToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value);
  v8::Handle<v8::Value> StringToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value);
  v8::Handle<v8::Value> XStringToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value);
  v8::Handle<v8::Value> NumToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value, unsigned len);
  v8::Handle<v8::Value> CharToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value, unsigned len);
  v8::Handle<v8::Value> ByteToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value, unsigned len);
  v8::Handle<v8::Value> IntToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value);
  v8::Handle<v8::Value> Int1ToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value);
  v8::Handle<v8::Value> Int2ToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value);
  v8::Handle<v8::Value> FloatToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value);
  v8::Handle<v8::Value> TimeToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value);
  v8::Handle<v8::Value> DateToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value);
  v8::Handle<v8::Value> BCDToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value);

  v8::Handle<v8::Value> GetParameter(const CHND container, const RFC_PARAMETER_DESC &desc, v8::Handle<v8::External> functionHandle);
  v8::Handle<v8::Value> GetField(const CHND container, const RFC_FIELD_DESC &desc, v8::Handle<v8::External> functionHandle);
  v8::Handle<v8::Value> GetValue(const CHND container, RFCTYPE type, const SAP_UC *name, unsigned len, v8::Handle<v8::External> functionHandle);
  v8::Handle<v8::Value> StructureToInternal(const CHND container, const SAP_UC *name, v8::Handle<v8::External> functionHandle);
  v8::Handle<v8::Value> StructureToInternal(const CHND container, const RFC_STRUCTURE_HANDLE struc, v8::Handle<v8::External> functionHandle);
  v8::Handle<v8::Object> StructureToObject(const RFC_STRUCTURE_HANDLE struc, v8::Handle<v8::External> functionHandle, v8::Handle<v8::Value> fieldDescriptions);
  v8::Handle<v8::Value> BuildFieldDescriptionMap(const RFC_STRUCTURE_HANDLE struc, v8::Handle<v8::External> functionHandle);
  v8::Handle<v8::Value> TableToInternal(const CHND container, const SAP_UC *name, v8::Handle<v8::External> functionHandle);
  v8::Handle<v8::Value> StringToInternal(const CHND container, const SAP_UC *name);
  v8::Handle<v8::Value> XStringToInternal(const CHND container, const SAP_UC *name);
  v8::Handle<v8::Value> NumToInternal(const CHND container, const SAP_UC *name, unsigned len);
  v8::Handle<v8::Value> CharToInternal(const CHND container, const SAP_UC *name, unsigned len);
  v8::Handle<v8::Value> ByteToInternal(const CHND container, const SAP_UC *name, unsigned len);
  v8::Handle<v8::Value> IntToInternal(const CHND container, const SAP_UC *name);
  v8::Handle<v8::Value> Int1ToInternal(const CHND container, const SAP_UC *name);
  v8::Handle<v8::Value> Int2ToInternal(const CHND container, const SAP_UC *name);
  v8::Handle<v8::Value> FloatToInternal(const CHND container, const SAP_UC *name);
  v8::Handle<v8::Value> DateToInternal(const CHND container, const SAP_UC *name);
  v8::Handle<v8::Value> TimeToInternal(const CHND container, const SAP_UC *name);
  v8::Handle<v8::Value> BCDToInternal(const CHND container, const SAP_UC *name);

  class InvocationBaton
  {
    public:
    InvocationBaton() : function(nullptr), functionHandle(nullptr) { };
    ~InvocationBaton() {
      if (!this->cbInvoke.IsEmpty()) {
        this->cbInvoke.Dispose();
        this->cbInvoke.Clear();
      }
    };

    static void DestroyFunctionHandle(v8::Persistent<v8::Value> value, void *parameters) {
      v8::Handle<v8::External> wrappedFunctionHandle = v8::Handle<v8::External>::Cast(value);
      RFC_FUNCTION_HANDLE functionHandle = static_cast<RFC_FUNCTION_HANDLE>(wrappedFunctionHandle->Value());
      RFC_ERROR_INFO errorInfo;
      RfcDestroyFunction(functionHandle, &errorInfo);
      value.Dispose();
    };

    Function *function;
    Connection *connection;
    RFC_FUNCTION_HANDLE functionHandle;
    v8::Persistent<v8::Function> cbInvoke;
    RFC_ERROR_INFO errorInfo;
  };

  static v8::Persistent<v8::FunctionTemplate> ctorTemplate;
  static v8::Persistent<v8::ObjectTemplate> structureTemplate;

  static v8::Handle<v8::Value> StructureGetter(v8::Local<v8::String> property, const v8::AccessorInfo &info) {
    v8::HandleScope scope;
    Function *function = static_cast<Function *>(info.This()->GetPointerFromInternalField(0));
    RFC_STRUCTURE_HANDLE struc = static_cast<RFC_STRUCTURE_HANDLE>(info.This()->GetPointerFromInternalField(1));
    v8::Handle<v8::External> functionHandle = v8::Handle<v8::External>::Cast(info.This()->GetInternalField(2));
    v8::Local<v8::Object> fieldDescriptions = v8::Local<v8::Object>::Cast(info.This()->GetInternalField(3));

    if (!fieldDescriptions->Has(property)) {
      v8::Handle<v8::Value> result;
      return result;
    }

    v8::Handle<v8::External> wrappedFieldDesc = v8::Handle<v8::External>::Cast(fieldDescriptions->Get(property));
    RFC_FIELD_DESC *fieldDesc = static_cast<RFC_FIELD_DESC *>(wrappedFieldDesc->Value());

    return scope.Close(function->GetField(struc, *fieldDesc, functionHandle));
  };

  static void DestroyFieldDesc(v8::Persistent<v8::Value> value, void *parameters) {
    v8::Handle<v8::External> wrappedFieldDesc = v8::Handle<v8::External>::Cast(value);
    RFC_FIELD_DESC *fieldDesc = static_cast<RFC_FIELD_DESC *>(wrappedFieldDesc->Value());
    if (fieldDesc) {
      delete fieldDesc;
    }
    value.Dispose();
  };

  //RFC_CONNECTION_HANDLE connectionHandle;
  Connection *connection;
  RFC_FUNCTION_DESC_HANDLE functionDescHandle;
};

#endif /* FUNCTION_H_ */
