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

#include "Function.h"
#include <cassert>
#include <sstream>
#include <limits>
#include <cstdint>

Nan::Persistent<v8::Function> Function::ctor;

Function::Function(): connection(nullptr), functionDescHandle(nullptr)
{
}

Function::~Function()
{
}

NAN_MODULE_INIT(Function::Init)
{
  Nan::HandleScope scope;
  v8::Local<v8::FunctionTemplate> ctorTemplate = Nan::New<v8::FunctionTemplate>(New);
  ctorTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  ctorTemplate->SetClassName(Nan::New("Function").ToLocalChecked());
  Nan::SetPrototypeMethod(ctorTemplate, "Invoke", Invoke);
  Nan::SetPrototypeMethod(ctorTemplate, "MetaData", MetaData);

  ctor.Reset(ctorTemplate->GetFunction());
  Nan::Set(target, Nan::New("Function").ToLocalChecked(), ctorTemplate->GetFunction());
}

v8::Local<v8::Value> Function::NewInstance(Connection &connection, const Nan::NAN_METHOD_ARGS_TYPE args)
{
  Nan::EscapableHandleScope scope;
  unsigned int parmCount;

  v8::Local<v8::Object> func = Nan::New(ctor)->NewInstance();
  Function *self = node::ObjectWrap::Unwrap<Function>(func);

  self->log(Levels::SILLY, "Function::NewInstance");

  // Save connection
  assert(self != nullptr);
  self->connection = &connection;

  // Lookup function interface
  v8::String::Value functionName(args[0]);
  self->functionDescHandle = RfcGetFunctionDesc(connection.GetConnectionHandle(), (const SAP_UC*)*functionName, &self->errorInfo);
  LOG_API(self, "RfcGetFunctionDesc");
  assert(self->errorInfo.code != RFC_INVALID_HANDLE);

  if (self->functionDescHandle == nullptr) {
    self->log(Levels::DBG, "Function::NewInstance: Function description handle is NULL.");
    return scope.Escape(RfcError(self->errorInfo));
  }

  RfcGetParameterCount(self->functionDescHandle, &parmCount, &self->errorInfo);
  LOG_API(self, "RfcGetParameterCount");
  if (self->errorInfo.code != RFC_OK) {
    self->log(Levels::DBG, "Function::NewInstance: RfcGetParameterCount unsuccessful");
    return scope.Escape(RfcError(self->errorInfo));
  }

  // Dynamically add parameters to JS object
  for (unsigned int i = 0; i < parmCount; i++) {
    RFC_PARAMETER_DESC parmDesc;

    RfcGetParameterDescByIndex(self->functionDescHandle, i, &parmDesc, &self->errorInfo);
    if (self->errorInfo.code != RFC_OK) {
      self->log(Levels::DBG, "Function::NewInstance: RfcGetParameterDescByIndex unsuccessful");
      return scope.Escape(RfcError(self->errorInfo));
    }
    func->Set(Nan::New((const uint16_t*)(parmDesc.name)).ToLocalChecked(), Nan::Null());
  }

  return scope.Escape(func);
}

NAN_METHOD(Function::New)
{
  if (!info.IsConstructCall()) {
    Nan::ThrowError("Invalid call format. Please use the 'new' operator.");
    return;
  }

  Function *self = new Function();
  self->Wrap(info.This());
  self->log(Levels::SILLY, "Function::New: Function instance constructed");

  info.GetReturnValue().Set(info.This());
}


NAN_METHOD(Function::Invoke)
{
  unsigned int parmCount;

  Function *self = node::ObjectWrap::Unwrap<Function>(info.This());
  assert(self != nullptr);
  self->log(Levels::SILLY, "Function::Invoke");

  if (info.Length() < 1) {
    Nan::ThrowError("Function expects 2 arguments");
    return;
  }
  if (!info[0]->IsObject()) {
    Nan::ThrowError("Argument 1 must be an object");
    return;
  }
  if (!info[1]->IsFunction()) {
    Nan::ThrowError("Argument 2 must be a function");
    return;
  }

  // Create baton to hold call context
  InvocationBaton *baton = new InvocationBaton();
  baton->function = self;
  baton->connection = self->connection;

  // Store callback
  baton->cbInvoke = new Nan::Callback(info[1].As<v8::Function>());

  baton->functionHandle = RfcCreateFunction(self->functionDescHandle, &self->errorInfo);
  LOG_API(self, "RfcCreateFunction");
  if (baton->functionHandle == nullptr) {
    delete baton;
    self->log(Levels::DBG, "Function::Invoke: RfcCreateFunction finished with error");
    RETURN_RFC_ERROR(self->errorInfo);
  }

  RfcGetParameterCount(self->functionDescHandle, &parmCount, &self->errorInfo);
  LOG_API(self, "RfcGetParameterCount");
  if (self->errorInfo.code != RFC_OK) {
    delete baton;
    self->log(Levels::DBG, "Function::Invoke: RfcGetParameterCount returned with error");
    RETURN_RFC_ERROR(self->errorInfo);
  }

  v8::Local<v8::Object> inputParm = info[0]->ToObject();

  for (unsigned int i = 0; i < parmCount; i++) {
    RFC_PARAMETER_DESC parmDesc;

    RfcGetParameterDescByIndex(self->functionDescHandle, i, &parmDesc, &self->errorInfo);
    LOG_API(self, "RfcGetParameterDescByIndex");
    if (self->errorInfo.code != RFC_OK) {
      delete baton;
      self->log(Levels::DBG, "Function::Invoke: RfcGetParameterDescByIndex finished with error");
      RETURN_RFC_ERROR(self->errorInfo);
    }

    v8::Local<v8::String> parmName = Nan::New((const uint16_t*)(parmDesc.name)).ToLocalChecked();
    v8::Local<v8::Value> result = Nan::Undefined();

    if (inputParm->Has(parmName) && !inputParm->Get(parmName)->IsNull()) {
      switch (parmDesc.direction) {
        case RFC_IMPORT:
        case RFC_CHANGING:
        case RFC_TABLES:
          result = self->SetValue(baton->functionHandle, parmDesc.type, parmDesc.name, parmDesc.nucLength, inputParm->Get(parmName));
          break;
        case RFC_EXPORT:
        default:
          break;
      }

      if (IsException(result)) {
        v8::Local<v8::Value> argv[2];
        argv[0] = result;
        argv[1] = Nan::Null();
        Nan::TryCatch try_catch;

        self->log(Levels::SILLY, "Function::Invoke: About call callback with error.");
        baton->cbInvoke->Call(Nan::GetCurrentContext()->Global(), 2, argv);
        delete baton;
        if (try_catch.HasCaught()) {
          self->log(Levels::ERR, "Function::Invoke: Callback has thrown. Aborting.");
          Nan::FatalException(try_catch);
        }
        info.GetReturnValue().SetUndefined();
        return;
      }
    }

    RfcSetParameterActive(baton->functionHandle, parmDesc.name, true, &self->errorInfo);
    LOG_API(self, "RfcSetParameterActive");
    if (self->errorInfo.code != RFC_OK) {
      delete baton;
      self->log(Levels::DBG, "Function::Invoke: RfcSetParameterActive returned error.");
      RETURN_RFC_ERROR(self->errorInfo);
    }
  }

  self->Ref();
  uv_work_t* req = new uv_work_t();
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, EIO_Invoke, (uv_after_work_cb)EIO_AfterInvoke);
#if !NODE_VERSION_AT_LEAST(0, 7, 9)
  uv_ref(uv_default_loop());
#endif

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(Function::MetaData)
{
  unsigned int parmCount;

  Function *self = node::ObjectWrap::Unwrap<Function>(info.This());
  assert(self != nullptr);

  self->log(Levels::SILLY, "Function::MetaData");

  RfcGetParameterCount(self->functionDescHandle, &parmCount, &self->errorInfo);
  LOG_API(self, "RfcGetParameterCount");
  if (self->errorInfo.code != RFC_OK) {
    self->log(Levels::DBG, "Function::MetaData: RfcGetParameterCount returned with error");
    RETURN_RFC_ERROR(self->errorInfo);
  }

  v8::Local<v8::Object> metaObject = Nan::New<v8::Object>();
  RFC_ABAP_NAME functionName;
  RfcGetFunctionName(self->functionDescHandle, functionName, &self->errorInfo);
  LOG_API(self, "RfcGetFunctionName");
  if (self->errorInfo.code != RFC_OK) {
    self->log(Levels::DBG, "Function::MetaData: RfcGetFunctionName returned with error");
    RETURN_RFC_ERROR(self->errorInfo);
  }

  RFC_FUNCTION_HANDLE functionHandle = RfcCreateFunction(self->functionDescHandle, &self->errorInfo);
  LOG_API(self, "RfcCreateFunction");
  if (functionHandle == nullptr) {
    self->log(Levels::DBG, "Function::MetaData: RfcCreateFunction finished with error");
    RfcDestroyFunction(functionHandle, &self->errorInfo);
    LOG_API(self, "RfcDestroyFunction");
    RETURN_RFC_ERROR(self->errorInfo);
  }

  std::string title = "Signature of SAP RFC function " + convertToString(functionName);

  metaObject->Set(Nan::New<v8::String>("title").ToLocalChecked(),
    Nan::New<v8::String>(title.c_str()).ToLocalChecked());
  metaObject->Set(Nan::New<v8::String>("type").ToLocalChecked(),
    Nan::New<v8::String>("object").ToLocalChecked());

  v8::Local<v8::Object> properties = Nan::New<v8::Object>();
  metaObject->Set(Nan::New<v8::String>("properties").ToLocalChecked(), properties);

  // Dynamically add parameters to JS object
  for (unsigned int i = 0; i < parmCount; i++) {
    RFC_PARAMETER_DESC parmDesc;

    RfcGetParameterDescByIndex(self->functionDescHandle, i, &parmDesc, &self->errorInfo);
    LOG_API(self, "RfcGetParameterDescByIndex");
    if (self->errorInfo.code != RFC_OK) {
      self->log(Levels::DBG, "Function::MetaData: RfcGetParameterDescByIndex finished with error");
      RETURN_RFC_ERROR(self->errorInfo);
    }

    if (!self->addMetaData(functionHandle, properties, parmDesc.name, parmDesc.type,
                           parmDesc.nucLength, parmDesc.direction, parmDesc.parameterText)) {
      RETURN_RFC_ERROR(self->errorInfo);
    }
  }

  RfcDestroyFunction(functionHandle, &self->errorInfo);
  LOG_API(self, "RfcDestroyFunction");

  info.GetReturnValue().Set(metaObject);
}

void Function::EIO_Invoke(uv_work_t *req)
{
  int isValid;

  InvocationBaton *baton = static_cast<InvocationBaton*>(req->data);

  assert(baton != nullptr);
  assert(baton->functionHandle != nullptr);
  assert(baton->function != nullptr);

  baton->connection->LockMutex();

  // Invocation
  RfcInvoke(baton->connection->GetConnectionHandle(), baton->functionHandle, &baton->function->errorInfo);
  DEFER_LOG_API(baton->function, "RfcInvoke");

  // If handle is invalid, fetch a better error message
  if (baton->function->errorInfo.code == RFC_INVALID_HANDLE) {
    RfcIsConnectionHandleValid(baton->connection->GetConnectionHandle(), &isValid, &baton->function->errorInfo);
    DEFER_LOG_API(baton->function, "RfcIsConnectionHandleValid");
  }

  baton->connection->UnlockMutex();
}

void Function::EIO_AfterInvoke(uv_work_t *req)
{
  Nan::HandleScope scope;

  InvocationBaton *baton = static_cast<InvocationBaton*>(req->data);
  assert(baton != nullptr);
  Function* self = baton->function;
  assert(self != nullptr);

  v8::Local<v8::Value> argv[2];
  argv[0] = Nan::Null();
  argv[1] = Nan::Null();

  if (self->errorInfo.code != RFC_OK) {
    argv[0] = RfcError(self->errorInfo);
  }

  v8::Local<v8::Value> result = self->DoReceive(baton->functionHandle);
  if (IsException(result)) {
    argv[0] = result;
  } else {
    argv[1] = result;
  }

  if (baton->functionHandle) {
    RfcDestroyFunction(baton->functionHandle, &self->errorInfo);
    LOG_API(self, "RfcDestroyFunction");
    baton->functionHandle = nullptr;
  }

  Nan::TryCatch try_catch;

  assert(!baton->cbInvoke.IsEmpty());
  self->log(Levels::SILLY, "Function::EIO_AfterInvoke: About to call callback");
  baton->cbInvoke->Call(Nan::GetCurrentContext()->Global(), 2, argv);

  delete baton;

  if (try_catch.HasCaught()) {
    self->log(Levels::ERR, "Function::EIO_AfterInvoke: Callback has thrown. Aborting.");
    Nan::FatalException(try_catch);
  }
}

v8::Local<v8::Value> Function::DoReceive(const CHND container)
{
  Nan::EscapableHandleScope scope;
  unsigned int parmCount;

  v8::Local<v8::Object> result = Nan::New<v8::Object>();

  // Get resulting values for exporting/changing/table parameters
  RfcGetParameterCount(this->functionDescHandle, &parmCount, &errorInfo);
  LOG_API(this, "RfcGetParameterCount");
  if (errorInfo.code != RFC_OK) {
    return scope.Escape(RfcError(errorInfo));
  }

  for (unsigned int i = 0; i < parmCount; i++) {
    RFC_PARAMETER_DESC parmDesc;
    v8::Local<v8::Value> parmValue;

    RfcGetParameterDescByIndex(this->functionDescHandle, i, &parmDesc, &errorInfo);
    LOG_API(this, "RfcGetParameterDescByIndex");
    if (errorInfo.code != RFC_OK) {
      return scope.Escape(RfcError(errorInfo));
    }

    switch (parmDesc.direction) {
      case RFC_IMPORT:
        //break;
      case RFC_CHANGING:
      case RFC_TABLES:
      case RFC_EXPORT:
        parmValue = this->GetValue(container, parmDesc.type, parmDesc.name, parmDesc.nucLength);
        if (IsException(parmValue)) {
          return scope.Escape(parmValue);
        }
        result->Set(Nan::New<v8::String>((const uint16_t*)parmDesc.name).ToLocalChecked(), parmValue);
        break;
      default:
        assert(0);
        break;
    }
  }

  return scope.Escape(result);
}

v8::Local<v8::Value> Function::SetValue(const CHND container, RFCTYPE type, const SAP_UC *name, unsigned len, v8::Local<v8::Value> value)
{
  Nan::EscapableHandleScope scope;

  v8::Local<v8::Value> result = Nan::Undefined();

  switch (type) {
    case RFCTYPE_DATE:
      result = this->DateToExternal(container, name, value);
      break;
    case RFCTYPE_TIME:
      result = this->TimeToExternal(container, name, value);
      break;
    case RFCTYPE_NUM:
      result = this->NumToExternal(container, name, value, len);
      break;
    case RFCTYPE_BCD:
      result = this->BCDToExternal(container, name, value);
      break;
    case RFCTYPE_CHAR:
      result = this->CharToExternal(container, name, value, len);
      break;
    case RFCTYPE_BYTE:
      result = this->ByteToExternal(container, name, value, len);
      break;
    case RFCTYPE_FLOAT:
      result = this->FloatToExternal(container, name, value);
      break;
    case RFCTYPE_INT:
      result = this->IntToExternal(container, name, value);
      break;
    case RFCTYPE_INT1:
      result = this->Int1ToExternal(container, name, value);
      break;
    case RFCTYPE_INT2:
      result = this->Int2ToExternal(container, name, value);
      break;
    case RFCTYPE_STRUCTURE:
      result = this->StructureToExternal(container, name, value);
      break;
    case RFCTYPE_TABLE:
      result = this->TableToExternal(container, name, value);
      break;
    case RFCTYPE_STRING:
      result = this->StringToExternal(container, name, value);
      break;
    case RFCTYPE_XSTRING:
      result = this->XStringToExternal(container, name, value);
      break;
    default:
      // Type not implemented
      return scope.Escape(RfcError("RFC type not implemented: ", Nan::New<v8::Uint32>(type)->ToString()));
      break;
  }

  if (IsException(result)) {
    return scope.Escape(result);
  }

  return scope.Escape(Nan::Null());
}

v8::Local<v8::Value> Function::StructureToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value)
{
  Nan::EscapableHandleScope scope;
  RFC_STRUCTURE_HANDLE strucHandle;

  RfcGetStructure(container, name, &strucHandle, &errorInfo);
  LOG_API(this, "RfcGetStructure");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(this->StructureToExternal(container, strucHandle, value));
}

v8::Local<v8::Value> Function::StructureToExternal(const CHND container, const RFC_STRUCTURE_HANDLE struc, v8::Local<v8::Value> value)
{
  Nan::EscapableHandleScope scope;
  RFC_TYPE_DESC_HANDLE typeHandle;
  RFC_FIELD_DESC fieldDesc;
  unsigned fieldCount;

  if (!value->IsObject()) {
    return ESCAPE_RFC_ERROR("Argument has unexpected type: ", fieldDesc.name);
  }
  v8::Local<v8::Object> valueObj = value->ToObject();

  typeHandle = RfcDescribeType(struc, &errorInfo);
  LOG_API(this, "RfcDescribeType");
  assert(typeHandle);
  if (typeHandle == nullptr) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  RfcGetFieldCount(typeHandle, &fieldCount, &errorInfo);
  LOG_API(this, "RfcGetFieldCount");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  for (unsigned int i = 0; i < fieldCount; i++) {
    RfcGetFieldDescByIndex(typeHandle, i, &fieldDesc, &errorInfo);
    LOG_API(this, "RfcGetFieldDescByIndex");
    if (errorInfo.code != RFC_OK) {
      return ESCAPE_RFC_ERROR(errorInfo);
    }

    v8::Local<v8::String> fieldName = Nan::New<v8::String>((const uint16_t*)(fieldDesc.name)).ToLocalChecked();

    if (valueObj->Has(fieldName)) {
      v8::Local<v8::Value> result = this->SetValue(struc, fieldDesc.type, fieldDesc.name, fieldDesc.nucLength, valueObj->Get(fieldName));
      // Bail out on exception
      if (IsException(result)) {
        return scope.Escape(result);
      }
    }
  }

  return scope.Escape(Nan::Null());
}

v8::Local<v8::Value> Function::TableToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value)
{
  Nan::EscapableHandleScope scope;
  RFC_TABLE_HANDLE tableHandle;
  RFC_STRUCTURE_HANDLE strucHandle;
  uint32_t rowCount;

  if (!value->IsArray()) {
    return ESCAPE_RFC_ERROR("Argument has unexpected type: ", name);
  }

  RfcGetTable(container, name, &tableHandle, &errorInfo);
  LOG_API(this, "RfcGetTable");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  v8::Local<v8::Array> source = v8::Local<v8::Array>::Cast(value);
  rowCount = source->Length();

  for (uint32_t i = 0; i < rowCount; i++){
    strucHandle = RfcAppendNewRow(tableHandle, &errorInfo);
    LOG_API(this, "RfcAppendNewRow");

    v8::Local<v8::Value> line = this->StructureToExternal(container, strucHandle, source->Get(i));
    // Bail out on exception
    if (IsException(line)) {
      return scope.Escape(line);
    }
  }

  return scope.Escape(Nan::Null());
}

v8::Local<v8::Value> Function::StringToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value)
{
  Nan::EscapableHandleScope scope;

  if (!value->IsString()) {
    return ESCAPE_RFC_ERROR("Argument has unexpected type: ", name);
  }

  v8::String::Value valueU16(value->ToString());
  RfcSetString(container, name, (const SAP_UC*)*valueU16, valueU16.length(), &errorInfo);
  LOG_API(this, "RfcSetString");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(Nan::Null());
}

v8::Local<v8::Value> Function::XStringToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value)
{
  Nan::EscapableHandleScope scope;

  if (!node::Buffer::HasInstance(value)) {
    return ESCAPE_RFC_ERROR("Argument has unexpected type: ", name);
  }

  unsigned int bufferLength = node::Buffer::Length(value);
  SAP_RAW* bufferData = reinterpret_cast<SAP_RAW*>(node::Buffer::Data(value));

  RfcSetXString(container, name, bufferData, bufferLength, &errorInfo);
  LOG_API(this, "RfcSetXString");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(Nan::Null());
}

v8::Local<v8::Value> Function::NumToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value, unsigned len)
{
  Nan::EscapableHandleScope scope;

  if (!value->IsString()) {
    return ESCAPE_RFC_ERROR("Argument has unexpected type: ", name);
  }

  v8::String::Value valueU16(value->ToString());
  if (valueU16.length() < 0 || (static_cast<unsigned int>(valueU16.length())) > len) {
    return ESCAPE_RFC_ERROR("Argument exceeds maximum length: ", name);
  }

  RfcSetNum(container, name, (const RFC_NUM*)*valueU16, valueU16.length(), &errorInfo);
  LOG_API(this, "RfcSetNum");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(Nan::Null());
}

v8::Local<v8::Value> Function::CharToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value, unsigned len)
{
  Nan::EscapableHandleScope scope;

  if (!value->IsString()) {
    return ESCAPE_RFC_ERROR("Argument has unexpected type: ", name);
  }

  v8::String::Value valueU16(value->ToString());
  if (valueU16.length() < 0 || (static_cast<unsigned int>(valueU16.length())) > len) {
    return ESCAPE_RFC_ERROR("Argument exceeds maximum length: ", name);
  }

  RfcSetChars(container, name, (const RFC_CHAR*)*valueU16, valueU16.length(), &errorInfo);
  LOG_API(this, "RfcSetChars");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(Nan::Null());
}

v8::Local<v8::Value> Function::ByteToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value, unsigned len)
{
  Nan::EscapableHandleScope scope;

  if (!node::Buffer::HasInstance(value)) {
    return ESCAPE_RFC_ERROR("Argument has unexpected type: ", name);
  }

  unsigned int bufferLength = node::Buffer::Length(value);
  if (bufferLength > len) {
    return ESCAPE_RFC_ERROR("Argument exceeds maximum length: ", name);
  }

  SAP_RAW* bufferData = reinterpret_cast<SAP_RAW*>(node::Buffer::Data(value));

  RfcSetBytes(container, name, bufferData, len, &errorInfo);
  LOG_API(this, "RfcSetBytes");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(Nan::Null());
}


v8::Local<v8::Value> Function::IntToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value)
{
  Nan::EscapableHandleScope scope;

  if (!value->IsInt32()) {
    return ESCAPE_RFC_ERROR("Argument has unexpected type: ", name);
  }
  RFC_INT rfcValue = value->ToInt32()->Value();

  RfcSetInt(container, name, rfcValue, &errorInfo);
  LOG_API(this, "RfcSetInt");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(Nan::Null());
}

v8::Local<v8::Value> Function::Int1ToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value)
{
  Nan::EscapableHandleScope scope;

  if (!value->IsInt32()) {
    return ESCAPE_RFC_ERROR("Argument has unexpected type: ", name);
  }
  int32_t convertedValue = value->ToInt32()->Value();
  if ((convertedValue < std::numeric_limits<int8_t>::min()) || (convertedValue > std::numeric_limits<int8_t>::max())) {
    return ESCAPE_RFC_ERROR("Argument out of range: ", name);
  }
  RFC_INT1 rfcValue = convertedValue;

  RfcSetInt1(container, name, rfcValue, &errorInfo);
  LOG_API(this, "RfcSetInt1");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(Nan::Null());
}

v8::Local<v8::Value> Function::Int2ToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value)
{
  Nan::EscapableHandleScope scope;

  if (!value->IsInt32()) {
    return ESCAPE_RFC_ERROR("Argument has unexpected type: ", name);
  }

  int32_t convertedValue = value->ToInt32()->Value();
  if ((convertedValue < std::numeric_limits<int16_t>::min()) || (convertedValue > std::numeric_limits<int16_t>::max())) {
    return ESCAPE_RFC_ERROR("Argument out of range: ", name);
  }
  RFC_INT2 rfcValue = convertedValue;

  RfcSetInt2(container, name, rfcValue, &errorInfo);
  LOG_API(this, "RfcSetInt2");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(Nan::Null());
}

v8::Local<v8::Value> Function::FloatToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value)
{
  Nan::EscapableHandleScope scope;

  if (!value->IsNumber()) {
    return ESCAPE_RFC_ERROR("Argument has unexpected type: ", name);
  }
  RFC_FLOAT rfcValue = value->ToNumber()->Value();

  RfcSetFloat(container, name, rfcValue, &errorInfo);
  LOG_API(this, "RfcSetFloat");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(Nan::Null());
}

v8::Local<v8::Value> Function::DateToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value)
{
  Nan::EscapableHandleScope scope;

  if (!value->IsString()) {
    return ESCAPE_RFC_ERROR("Argument has unexpected type: ", name);
  }

  v8::Local<v8::String> str = value->ToString();
  if (str->Length() != 8) {
    return ESCAPE_RFC_ERROR("Invalid date format: ", name);
  }

  v8::String::Value rfcValue(str);
  assert(*rfcValue);
  RfcSetDate(container, name, (const RFC_CHAR*)*rfcValue, &errorInfo);
  LOG_API(this, "RfcSetDate");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(Nan::Null());
}

v8::Local<v8::Value> Function::TimeToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value)
{
  Nan::EscapableHandleScope scope;

  if (!value->IsString()) {
    return ESCAPE_RFC_ERROR("Argument has unexpected type: ", name);
  }

  v8::Local<v8::String> str = value->ToString();
  if (str->Length() != 6) {
    return ESCAPE_RFC_ERROR("Invalid time format: ", name);
  }

  v8::String::Value rfcValue(str);
  assert(*rfcValue);
  RfcSetTime(container, name, (const RFC_CHAR*)*rfcValue, &errorInfo);
  LOG_API(this, "RfcSetTime");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(Nan::Null());
}

v8::Local<v8::Value> Function::BCDToExternal(const CHND container, const SAP_UC *name, v8::Local<v8::Value> value)
{
  Nan::EscapableHandleScope scope;

  if (!value->IsNumber()) {
    return ESCAPE_RFC_ERROR("Argument has unexpected type: ", name);
  }

  v8::String::Value valueU16(value->ToString());

  RfcSetString(container, name, (const SAP_UC*)*valueU16, valueU16.length(), &errorInfo);
  LOG_API(this, "RfcSetString");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(Nan::Null());
}

v8::Local<v8::Value> Function::GetValue(const CHND container, RFCTYPE type, const SAP_UC *name, unsigned len)
{
  Nan::EscapableHandleScope scope;
  v8::Local<v8::Value> value = Nan::Null();

  switch (type) {
    case RFCTYPE_DATE:
      value = this->DateToInternal(container, name);
      break;
    case RFCTYPE_TIME:
      value = this->TimeToInternal(container, name);
      break;
    case RFCTYPE_NUM:
      value = this->NumToInternal(container, name, len);
      break;
    case RFCTYPE_BCD:
      value = this->BCDToInternal(container, name);
      break;
    case RFCTYPE_CHAR:
      value = this->CharToInternal(container, name, len);
      break;
    case RFCTYPE_BYTE:
      value = this->ByteToInternal(container, name, len);
      break;
    case RFCTYPE_FLOAT:
      value = this->FloatToInternal(container, name);
      break;
    case RFCTYPE_INT:
      value = this->IntToInternal(container, name);
      break;
    case RFCTYPE_INT1:
      value = this->Int1ToInternal(container, name);
      break;
    case RFCTYPE_INT2:
      value = this->Int2ToInternal(container, name);
      break;
    case RFCTYPE_STRUCTURE:
      value = this->StructureToInternal(container, name);
      break;
    case RFCTYPE_TABLE:
      value = this->TableToInternal(container, name);
      break;
    case RFCTYPE_STRING:
      value = this->StringToInternal(container, name);
      break;
    case RFCTYPE_XSTRING:
      value = this->XStringToInternal(container, name);
      break;
    default:
      // Type not implemented
      return ESCAPE_RFC_ERROR("RFC type not implemented: ", Nan::New<v8::Uint32>(type)->ToString());
      break;
  }

  return scope.Escape(value);
}

v8::Local<v8::Value> Function::StructureToInternal(const CHND container, const SAP_UC *name)
{
  Nan::EscapableHandleScope scope;
  RFC_STRUCTURE_HANDLE strucHandle;

  RfcGetStructure(container, name, &strucHandle, &errorInfo);
  LOG_API(this, "RfcGetStructure");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(this->StructureToInternal(container, strucHandle));
}

v8::Local<v8::Value> Function::StructureToInternal(const CHND container, const RFC_STRUCTURE_HANDLE struc)
{
  Nan::EscapableHandleScope scope;
  RFC_TYPE_DESC_HANDLE typeHandle;
  RFC_FIELD_DESC fieldDesc;
  unsigned fieldCount;

  typeHandle = RfcDescribeType(struc, &errorInfo);
  LOG_API(this, "RfcDescribeType");
  assert(typeHandle);
  if (typeHandle == nullptr) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  RfcGetFieldCount(typeHandle, &fieldCount, &errorInfo);
  LOG_API(this, "RfcGetFieldCount");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  v8::Local<v8::Object> obj = Nan::New<v8::Object>();

  for (unsigned int i = 0; i < fieldCount; i++) {
    RfcGetFieldDescByIndex(typeHandle, i, &fieldDesc, &errorInfo);
    LOG_API(this, "RfcGetFieldDescByIndex");
    if (errorInfo.code != RFC_OK) {
      return ESCAPE_RFC_ERROR(errorInfo);
    }

    v8::Local<v8::Value> value = this->GetValue(struc, fieldDesc.type, fieldDesc.name, fieldDesc.nucLength);
    // Bail out on exception
    if (IsException(value)) {
      return scope.Escape(value);
    }
    obj->Set(Nan::New<v8::String>((const uint16_t*)(fieldDesc.name)).ToLocalChecked(), value);
  }

  return scope.Escape(obj);
}

v8::Local<v8::Value> Function::TableToInternal(const CHND container, const SAP_UC *name)
{
  Nan::EscapableHandleScope scope;
  RFC_TABLE_HANDLE tableHandle;
  RFC_STRUCTURE_HANDLE strucHandle;
  unsigned rowCount;

  RfcGetTable(container, name, &tableHandle, &errorInfo);
  LOG_API(this, "RfcGetTable");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  RfcGetRowCount(tableHandle, &rowCount, &errorInfo);
  LOG_API(this, "RfcGetRowCount");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  // Create array holding table lines
  v8::Local<v8::Array> obj = Nan::New<v8::Array>();

  for (unsigned int i = 0; i < rowCount; i++){
    RfcMoveTo(tableHandle, i, &errorInfo);
    LOG_API(this, "RfcMoveTo");
    strucHandle = RfcGetCurrentRow(tableHandle, &errorInfo);
    LOG_API(this, "RfcGetCurrentRow");

    v8::Local<v8::Value> line = this->StructureToInternal(container, strucHandle);
    // Bail out on exception
    if (IsException(line)) {
      return scope.Escape(line);
    }
    obj->Set(i, line);
  }

  return scope.Escape(obj);
}

v8::Local<v8::Value> Function::StringToInternal(const CHND container, const SAP_UC *name)
{
  Nan::EscapableHandleScope scope;
  unsigned strLen, retStrLen;

  RfcGetStringLength(container, name, &strLen, &errorInfo);
  LOG_API(this, "RfcGetStringLength");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  if (strLen == 0) {
    return scope.Escape(Nan::EmptyString());
  }

  SAP_UC *buffer = static_cast<SAP_UC*>(malloc((strLen + 1) * sizeof(SAP_UC)));
  assert(buffer);
  memset(buffer, 0, (strLen + 1) * sizeof(SAP_UC));

  RfcGetString(container, name, buffer, strLen + 1, &retStrLen, &errorInfo);
  LOG_API(this, "RfcGetString");
  if (errorInfo.code != RFC_OK) {
    free(buffer);
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  v8::Local<v8::String> value = Nan::New<v8::String>((const uint16_t*)(buffer)).ToLocalChecked();

  free(buffer);

  return scope.Escape(value);
}

v8::Local<v8::Value> Function::XStringToInternal(const CHND container, const SAP_UC *name)
{
  Nan::EscapableHandleScope scope;
  unsigned strLen, retStrLen;

  RfcGetStringLength(container, name, &strLen, &errorInfo);
  LOG_API(this, "RfcGetStringLength");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  if (strLen == 0) {
    return scope.Escape(Nan::EmptyString());
  }

  SAP_RAW *buffer = static_cast<SAP_RAW*>(malloc(strLen * sizeof(SAP_RAW)));
  assert(buffer);
  memset(buffer, 0, strLen * sizeof(SAP_RAW));

  RfcGetXString(container, name, buffer, strLen, &retStrLen, &errorInfo);
  LOG_API(this, "RfcGetXString");
  if (errorInfo.code != RFC_OK) {
    free(buffer);
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  v8::Local<v8::Value> value = Nan::NewBuffer(reinterpret_cast<char*>(buffer), strLen).ToLocalChecked();

  return scope.Escape(value);
}

v8::Local<v8::Value> Function::NumToInternal(const CHND container, const SAP_UC *name, unsigned len)
{
  Nan::EscapableHandleScope scope;

  RFC_NUM *buffer = static_cast<RFC_NUM*>(malloc((len + 1) * sizeof(RFC_NUM)));
  assert(buffer);
  memset(buffer, 0, (len + 1) * sizeof(RFC_NUM));

  RfcGetNum(container, name, buffer, len, &errorInfo);
  LOG_API(this, "RfcGetNum");
  if (errorInfo.code != RFC_OK) {
    free(buffer);
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  v8::Local<v8::String> value = Nan::New<v8::String>((const uint16_t*)(buffer)).ToLocalChecked();

  free(buffer);

  return scope.Escape(value);
}

v8::Local<v8::Value> Function::CharToInternal(const CHND container, const SAP_UC *name, unsigned len)
{
  Nan::EscapableHandleScope scope;

  RFC_CHAR *buffer = static_cast<RFC_CHAR*>(malloc((len + 1) * sizeof(RFC_CHAR)));
  assert(buffer);
  memset(buffer, 0, (len + 1) * sizeof(RFC_CHAR));

  RfcGetChars(container, name, buffer, len, &errorInfo);
  LOG_API(this, "RfcGetChars");
  if (errorInfo.code != RFC_OK) {
    free(buffer);
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  v8::Local<v8::String> value = Nan::New<v8::String>((const uint16_t*)(buffer)).ToLocalChecked();

  free(buffer);

  return scope.Escape(value);
}

v8::Local<v8::Value> Function::ByteToInternal(const CHND container, const SAP_UC *name, unsigned len)
{
  Nan::EscapableHandleScope scope;

  RFC_BYTE *buffer = static_cast<RFC_BYTE*>(malloc(len * sizeof(RFC_BYTE)));
  assert(buffer);
  memset(buffer, 0, len * sizeof(RFC_BYTE));

  RfcGetBytes(container, name, buffer, len, &errorInfo);
  LOG_API(this, "RfcGetBytes");
  if (errorInfo.code != RFC_OK) {
    free(buffer);
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  v8::Local<v8::Value> value = Nan::NewBuffer(reinterpret_cast<char*>(buffer), len).ToLocalChecked();

  return scope.Escape(value);
}

v8::Local<v8::Value> Function::IntToInternal(const CHND container, const SAP_UC *name)
{
  Nan::EscapableHandleScope scope;
  RFC_INT value;

  RfcGetInt(container, name, &value, &errorInfo);
  LOG_API(this, "RfcGetInt");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(Nan::New<v8::Integer>(value));
}

v8::Local<v8::Value> Function::Int1ToInternal(const CHND container, const SAP_UC *name)
{
  Nan::EscapableHandleScope scope;
  RFC_INT1 value;

  RfcGetInt1(container, name, &value, &errorInfo);
  LOG_API(this, "RfcGetInt1");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(Nan::New<v8::Integer>(value));
}

v8::Local<v8::Value> Function::Int2ToInternal(const CHND container, const SAP_UC *name)
{
  Nan::EscapableHandleScope scope;
  RFC_INT2 value;

  RfcGetInt2(container, name, &value, &errorInfo);
  LOG_API(this, "RfcGetInt2");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(Nan::New<v8::Integer>(value));
}

v8::Local<v8::Value> Function::FloatToInternal(const CHND container, const SAP_UC *name)
{
  Nan::EscapableHandleScope scope;
  RFC_FLOAT value;

  RfcGetFloat(container, name, &value, &errorInfo);
  LOG_API(this, "RfcGetFloat");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  return scope.Escape(Nan::New<v8::Number>(value));
}

v8::Local<v8::Value> Function::DateToInternal(const CHND container, const SAP_UC *name)
{
  Nan::EscapableHandleScope scope;
  RFC_DATE date = { 0 };

  RfcGetDate(container, name, date, &errorInfo);
  LOG_API(this, "RfcGetDate");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  assert(sizeof(RFC_CHAR) > 0); // Shouldn't occur except in case of a compiler glitch
  v8::Local<v8::String> value = Nan::New<v8::String>(
    (const uint16_t*)(date), (unsigned)(sizeof(RFC_DATE) / sizeof(RFC_CHAR))).ToLocalChecked();

  return scope.Escape(value);
}

v8::Local<v8::Value> Function::TimeToInternal(const CHND container, const SAP_UC *name)
{
  Nan::EscapableHandleScope scope;
  RFC_TIME time = { 0 };

  RfcGetTime(container, name, time, &errorInfo);
  LOG_API(this, "RfcGetTime");
  if (errorInfo.code != RFC_OK) {
    return ESCAPE_RFC_ERROR(errorInfo);
  }

  assert(sizeof(RFC_CHAR) > 0); // Shouldn't occur except in case of a compiler glitch
  v8::Local<v8::String> value = Nan::New<v8::String>(
    (const uint16_t*)(time), (unsigned)(sizeof(RFC_TIME) / sizeof(RFC_CHAR))).ToLocalChecked();

  return scope.Escape(value);
}

v8::Local<v8::Value> Function::BCDToInternal(const CHND container, const SAP_UC *name)
{
  Nan::EscapableHandleScope scope;
  unsigned strLen = 25;
  unsigned retStrLen;
  SAP_UC *buffer;

  do {
    buffer = static_cast<SAP_UC*>(malloc((strLen + 1) * sizeof(SAP_UC)));
    assert(buffer);
    memset(buffer, 0, (strLen + 1) * sizeof(SAP_UC));

    RfcGetString(container, name, buffer, strLen + 1, &retStrLen, &errorInfo);
    LOG_API(this, "RfcGetString");

    if (errorInfo.code == RFC_BUFFER_TOO_SMALL) {
      // Retry with suggested string length
      free(buffer);
      strLen = retStrLen;
      log(Levels::SILLY, "Function::BCDToInternal: Retry");
    } else if (errorInfo.code != RFC_OK) {
      free(buffer);
      return ESCAPE_RFC_ERROR(errorInfo);
    }
  } while (errorInfo.code == RFC_BUFFER_TOO_SMALL);

  v8::Local<v8::String> value = Nan::New<v8::String>((const uint16_t*)(buffer), retStrLen).ToLocalChecked();

  free(buffer);

  return scope.Escape(value->ToNumber());
}

std::string Function::mapExternalTypeToJavaScriptType(RFCTYPE sapType)
{
  switch (sapType) {
    case RFCTYPE_CHAR:
    case RFCTYPE_DATE:
    case RFCTYPE_TIME:
    case RFCTYPE_BYTE:
    case RFCTYPE_NUM:
    case RFCTYPE_STRING:
    case RFCTYPE_XSTRING:
      return "string";
    case RFCTYPE_TABLE:
      return "array";
    case RFCTYPE_ABAPOBJECT:
    case RFCTYPE_STRUCTURE:
      return "object";
    case RFCTYPE_BCD:
    case RFCTYPE_FLOAT:
    case RFCTYPE_DECF16:
    case RFCTYPE_DECF34:
      return "number";
    case RFCTYPE_INT:
    case RFCTYPE_INT2:
    case RFCTYPE_INT1:
    case RFCTYPE_INT8:
    case RFCTYPE_UTCLONG:
    case RFCTYPE_UTCSECOND:
    case RFCTYPE_UTCMINUTE:
    case RFCTYPE_DTDAY:
    case RFCTYPE_DTWEEK:
    case RFCTYPE_DTMONTH:
    case RFCTYPE_TSECOND:
    case RFCTYPE_TMINUTE:
    case RFCTYPE_CDAY:
      return "integer";
    default:
      return "undefined";
  }

}

bool Function::addMetaData(const CHND container, v8::Local<v8::Object> &parent,
                           const RFC_ABAP_NAME name, RFCTYPE type, unsigned int length,
                           RFC_DIRECTION direction, RFC_PARAMETER_TEXT paramText)
{
  Nan::EscapableHandleScope scope;

  log(Levels::SILLY, "Function::addMetaData");

  v8::Local<v8::Object> actualType = Nan::New<v8::Object>();
  parent->Set(Nan::New<v8::String>((const uint16_t*)name).ToLocalChecked(), actualType);

  actualType->Set(
    Nan::New<v8::String>("type").ToLocalChecked(),
    Nan::New<v8::String>(mapExternalTypeToJavaScriptType(type).c_str()).ToLocalChecked());

  std::stringstream lengthString;
  lengthString << length;
  actualType->Set(
    Nan::New<v8::String>("length").ToLocalChecked(),
    Nan::New<v8::String>(lengthString.str().c_str()).ToLocalChecked());

  actualType->Set(
    Nan::New<v8::String>("sapType").ToLocalChecked(),
    Nan::New<v8::String>((uint16_t*)RfcGetTypeAsString(type)).ToLocalChecked());

  if (paramText != nullptr) {
    actualType->Set(
        Nan::New<v8::String>("description").ToLocalChecked(),
        Nan::New<v8::String>((const uint16_t*)paramText).ToLocalChecked());
  }

  if (direction != 0) {
    actualType->Set(
        Nan::New<v8::String>("sapDirection").ToLocalChecked(),
        Nan::New<v8::String>((uint16_t*)RfcGetDirectionAsString(direction)).ToLocalChecked());
  }

  if (type == RFCTYPE_STRUCTURE) {
    RFC_STRUCTURE_HANDLE strucHandle;
    RFC_TYPE_DESC_HANDLE typeHandle;
    RFC_FIELD_DESC fieldDesc;
    unsigned fieldCount;
    RFC_ABAP_NAME typeName;

    RfcGetStructure(container, name, &strucHandle, &errorInfo);
    LOG_API(this, "RfcGetStructure");
    if (errorInfo.code != RFC_OK) {
      return false;
    }

    typeHandle = RfcDescribeType(strucHandle, &errorInfo);
    LOG_API(this, "RfcDescribeType");
    assert(typeHandle);
    if (typeHandle == nullptr) {
      return false;
    }

    RfcGetTypeName(typeHandle, typeName, &errorInfo);
    LOG_API(this, "RfcGetTypeName");
    if (errorInfo.code != RFC_OK) {
      return false;
    }

    actualType->Set(
        Nan::New<v8::String>("sapTypeName").ToLocalChecked(),
        Nan::New<v8::String>((const uint16_t*)typeName).ToLocalChecked());

    RfcGetFieldCount(typeHandle, &fieldCount, &errorInfo);
    LOG_API(this, "RfcGetFieldCount");
    if (errorInfo.code != RFC_OK) {
      return false;
    }

    v8::Local<v8::Object> properties = Nan::New<v8::Object>();
    actualType->Set(Nan::New<v8::String>("properties").ToLocalChecked(), properties);

    for (unsigned int i = 0; i < fieldCount; i++) {
      RfcGetFieldDescByIndex(typeHandle, i, &fieldDesc, &errorInfo);
      LOG_API(this, "RfcGetFieldDescByIndex");
      if (errorInfo.code != RFC_OK) {
        return false;
      }

      if (!addMetaData( strucHandle, properties, fieldDesc.name, fieldDesc.type,
                        fieldDesc.nucLength, RFC_DIRECTION(0))) {
        return false;
      }
    }
  }
  else if (type == RFCTYPE_TABLE) {
    RFC_TABLE_HANDLE tableHandle;
    RFC_TYPE_DESC_HANDLE typeHandle;
    RFC_FIELD_DESC fieldDesc;
    unsigned fieldCount;
    RFC_ABAP_NAME typeName;

    RfcGetTable(container, name, &tableHandle, &errorInfo);
    LOG_API(this, "RfcGetTable");
    if (errorInfo.code != RFC_OK) {
      return false;
    }

    typeHandle = RfcDescribeType(tableHandle, &errorInfo);
    LOG_API(this, "RfcDescribeType");
    assert(typeHandle);
    if (typeHandle == nullptr) {
      return false;
    }

    RfcGetTypeName(typeHandle, typeName, &errorInfo);
    LOG_API(this, "RfcGetTypeName");
    if (errorInfo.code != RFC_OK) {
      return false;
    }

    RfcGetFieldCount(typeHandle, &fieldCount, &errorInfo);
    LOG_API(this, "RfcGetFieldCount");
    if (errorInfo.code != RFC_OK) {
      return false;
    }

    v8::Local<v8::Object> items = Nan::New<v8::Object>();
    actualType->Set(Nan::New<v8::String>("items").ToLocalChecked(), items);
    items->Set(
        Nan::New<v8::String>("sapTypeName").ToLocalChecked(),
        Nan::New<v8::String>((const uint16_t*)typeName).ToLocalChecked());

    items->Set(
        Nan::New<v8::String>("type").ToLocalChecked(),
        Nan::New<v8::String>("object").ToLocalChecked());

    v8::Local<v8::Object> properties = Nan::New<v8::Object>();
    items->Set(Nan::New<v8::String>("properties").ToLocalChecked(), properties);

    RFC_STRUCTURE_HANDLE rowHandle = RfcAppendNewRow(tableHandle, &errorInfo);
    LOG_API(this, "RfcAppendNewRow");
    if (errorInfo.code != RFC_OK) {
      return false;
    }

    for (unsigned int i = 0; i < fieldCount; i++) {
      RfcGetFieldDescByIndex(typeHandle, i, &fieldDesc, &errorInfo);
      LOG_API(this, "RfcGetFieldDescByIndex");
      if (errorInfo.code != RFC_OK) {
        return false;
      }

      log(Levels::SILLY, "Function::addMetaData recurse");
      if (!addMetaData( rowHandle, properties, fieldDesc.name, fieldDesc.type,
                   fieldDesc.nucLength, RFC_DIRECTION(0))) {
        return false;
      }
    }
  }

  return true;
}
