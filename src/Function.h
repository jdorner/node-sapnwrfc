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

#include <node.h>
#include <v8.h>
#include <node_version.h>
#include <sapnwrfc.h>
#include <pthread.h>

#include "Common.h"
#include "Connection.h"

typedef DATA_CONTAINER_HANDLE CHND;

class Function : public node::ObjectWrap
{
  public:
  static void Init(v8::Handle<v8::Object> target);
  static v8::Handle<v8::Value> NewInstance(const RFC_CONNECTION_HANDLE handle, const v8::Arguments &args);

  protected:
  Function();
  ~Function();
  
  static v8::Handle<v8::Value> New(const v8::Arguments &args);
  static v8::Handle<v8::Value> Invoke(const v8::Arguments &args);

#if NODE_VERSION_AT_LEAST(0, 5, 4)
  static void EIO_Invoke(eio_req *req);
#else
  static int EIO_Invoke(eio_req *req);
#endif
  static int EIO_AfterInvoke(eio_req *req);

  v8::Handle<v8::Value> DoReceive(const CHND container);

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

  v8::Handle<v8::Value> GetParameter(const CHND container, const RFC_PARAMETER_DESC &desc);
  v8::Handle<v8::Value> GetField(const CHND container, const RFC_FIELD_DESC &desc);
  v8::Handle<v8::Value> GetValue(const CHND container, RFCTYPE type, const SAP_UC *name, unsigned len);
  v8::Handle<v8::Value> StructureToInternal(const CHND container, const SAP_UC *name);
  v8::Handle<v8::Value> StructureToInternal(const CHND container, const RFC_STRUCTURE_HANDLE struc);
  v8::Handle<v8::Value> TableToInternal(const CHND container, const SAP_UC *name);
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
      RFC_ERROR_INFO errorInfo;
    
      if (this->functionHandle) {
        RfcDestroyFunction(this->functionHandle, &errorInfo);
        this->functionHandle = nullptr;
      }

      if (!this->cbInvoke.IsEmpty()) {
        this->cbInvoke.Dispose();
        this->cbInvoke.Clear();
      }
    };

    Function *function;
    RFC_CONNECTION_HANDLE connectionHandle;
    RFC_FUNCTION_HANDLE functionHandle;
    v8::Persistent<v8::Function> cbInvoke;
    RFC_ERROR_INFO errorInfo;
  };

  static v8::Persistent<v8::FunctionTemplate> ctorTemplate;
  static pthread_mutex_t invocationMutex;
  
  RFC_CONNECTION_HANDLE connectionHandle;
  RFC_FUNCTION_DESC_HANDLE functionDescHandle;
};

#endif /* FUNCTION_H_ */
