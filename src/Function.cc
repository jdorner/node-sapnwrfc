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

v8::Persistent<v8::FunctionTemplate> Function::ctorTemplate;

Function::Function(): connection(nullptr), functionDescHandle(nullptr)
{
}

Function::~Function()
{
}

void Function::Init(v8::Handle<v8::Object> target)
{
  v8::HandleScope scope;
  
  ctorTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(New));
  ctorTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  ctorTemplate->SetClassName(v8::String::NewSymbol("Function"));

  NODE_SET_PROTOTYPE_METHOD(ctorTemplate, "Invoke", Function::Invoke);

  target->Set(v8::String::NewSymbol("Function"), ctorTemplate->GetFunction());
}

v8::Handle<v8::Value> Function::NewInstance(Connection &connection, const v8::Arguments &args)
{
  v8::HandleScope scope;
  unsigned int parmCount;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;
  
  v8::Local<v8::Object> func = ctorTemplate->GetFunction()->NewInstance();
  Function *self = node::ObjectWrap::Unwrap<Function>(func);

  // Save connection
  assert(self != nullptr);
  self->connection = &connection;

  // Lookup function interface
  v8::String::Value functionName(args[0]);
  self->functionDescHandle = RfcGetFunctionDesc(connection.GetConnectionHandle(), (const SAP_UC*)*functionName, &errorInfo);
#ifndef NDEBUG
  if (errorInfo.code == RFC_INVALID_HANDLE) {
    assert(0);
  }
#endif

  if (self->functionDescHandle == nullptr) {
    return RFC_ERROR(errorInfo);
  }
  
  rc = RfcGetParameterCount(self->functionDescHandle, &parmCount, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  // Dynamically add parameters to JS object
  for (unsigned int i = 0; i < parmCount; i++) {
    RFC_PARAMETER_DESC parmDesc;
    
    rc = RfcGetParameterDescByIndex(self->functionDescHandle, i, &parmDesc, &errorInfo);
    if (rc != RFC_OK) {
      return RFC_ERROR(errorInfo);
    }
    func->Set(v8::String::New((const uint16_t*)(parmDesc.name)), v8::Null());
  }

  return func;
}

v8::Handle<v8::Value> Function::New(const v8::Arguments &args)
{
  v8::HandleScope scope;

  if (!args.IsConstructCall()) {
    return THROW_V8_EXCEPTION("Invalid call format. Please use the 'new' operator.");
  }

  Function *self = new Function();
  self->Wrap(args.This());

  return args.This();
}

v8::Handle<v8::Value> Function::Invoke(const v8::Arguments &args)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  unsigned int parmCount;
  RFC_ERROR_INFO errorInfo;
  
  Function *self = node::ObjectWrap::Unwrap<Function>(args.This());
  assert(self != nullptr);

  if (args.Length() < 1) {
    return THROW_V8_EXCEPTION("Function requires 2 arguments");
  }
  if (!args[0]->IsObject()) {
    return THROW_V8_EXCEPTION("Argument 1 must be an object");
  }
  if (!args[1]->IsFunction()) {
    return THROW_V8_EXCEPTION("Argument 2 must be a function");
  }

  // Create baton to hold call context
  InvocationBaton *baton = new InvocationBaton();
  baton->function = self;
  baton->connection = self->connection;

  // Store callback
  baton->cbInvoke = v8::Persistent<v8::Function>::New(v8::Local<v8::Function>::Cast(args[1]));
  
  baton->functionHandle = RfcCreateFunction(self->functionDescHandle, &errorInfo);
  if (baton->functionHandle == nullptr) {
    delete baton;
    return RFC_ERROR(errorInfo);
  }

  rc = RfcGetParameterCount(self->functionDescHandle, &parmCount, &errorInfo);
  if (rc != RFC_OK) {
    delete baton;
    return RFC_ERROR(errorInfo);
  }

  v8::Local<v8::Object> inputParm = args[0]->ToObject();

  for (unsigned int i = 0; i < parmCount; i++) {
    RFC_PARAMETER_DESC parmDesc;
    
    rc = RfcGetParameterDescByIndex(self->functionDescHandle, i, &parmDesc, &errorInfo);
    if (rc != RFC_OK) {
      delete baton;
      return RFC_ERROR(errorInfo);
    }

    v8::Local<v8::String> parmName = v8::String::New((const uint16_t*)(parmDesc.name));
    v8::Handle<v8::Value> result = v8::Undefined();

    if (inputParm->Has(parmName) && !inputParm->Get(parmName)->IsNull()) {
      switch (parmDesc.direction) {
        case RFC_IMPORT:
        case RFC_CHANGING:
        case RFC_TABLES:
          result = self->SetParameter(baton->functionHandle, parmDesc, inputParm->Get(parmName));
          break;
        case RFC_EXPORT:
        default:
          break;
      }

      if (IsException(result)) {
        v8::Local<v8::Value> argv[2];
        argv[0] = v8::Local<v8::Value>::New(result);
        argv[1] = v8::Local<v8::Value>::New(v8::Null());
        v8::TryCatch try_catch;

        baton->cbInvoke->Call(v8::Context::GetCurrent()->Global(), 2, argv);
        delete baton;
        if (try_catch.HasCaught()) {
          node::FatalException(try_catch);
        }
        return scope.Close(v8::Undefined());
      }
    }

    rc = RfcSetParameterActive(baton->functionHandle, parmDesc.name, true, &errorInfo);
    if (rc != RFC_OK) {
      delete baton;
      return RFC_ERROR(errorInfo);
    }
  }

  self->Ref();
  uv_work_t* req = new uv_work_t();
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, EIO_Invoke, (uv_after_work_cb)EIO_AfterInvoke);
#if !NODE_VERSION_AT_LEAST(0, 7, 9)
  uv_ref(uv_default_loop());
#endif

  return scope.Close(v8::Undefined());
}

void Function::EIO_Invoke(uv_work_t *req)
{
  RFC_RC rc = RFC_OK;
  int isValid;
  
  InvocationBaton *baton = static_cast<InvocationBaton*>(req->data);

  assert(baton != nullptr);
  assert(baton->functionHandle != nullptr);

  baton->connection->LockMutex();

  // Invocation
  rc = RfcInvoke(baton->connection->GetConnectionHandle(), baton->functionHandle, &baton->errorInfo);

  // If handle is invalid, fetch a better error message
  if (baton->errorInfo.code == RFC_INVALID_HANDLE) {
    rc = RfcIsConnectionHandleValid(baton->connection->GetConnectionHandle(), &isValid, &baton->errorInfo);
  }
  
  baton->connection->UnlockMutex();
}

void Function::EIO_AfterInvoke(uv_work_t *req)
{
  v8::HandleScope scope;
  RFC_ERROR_INFO errorInfo;
 
  InvocationBaton *baton = static_cast<InvocationBaton*>(req->data);
  assert(baton != nullptr);
  
  v8::Local<v8::Value> argv[2];
  argv[0] = v8::Local<v8::Value>::New(v8::Null());
  argv[1] = v8::Local<v8::Value>::New(v8::Null());
  
  if (baton->errorInfo.code != RFC_OK) {
    argv[0] = v8::Local<v8::Value>::New(RfcError(baton->errorInfo));
  } else {
    v8::Local<v8::Value> result = v8::Local<v8::Value>::New(baton->function->DoReceive(baton->functionHandle));
    if (IsException(result)) {
      argv[0] = result;
    } else {
      argv[1] = result;
    }
  }

  if (baton->functionHandle) {
    RfcDestroyFunction(baton->functionHandle, &errorInfo);
    baton->functionHandle = nullptr;
  }

  v8::TryCatch try_catch;

  assert(!baton->cbInvoke.IsEmpty());
  baton->cbInvoke->Call(v8::Context::GetCurrent()->Global(), 2, argv);

  baton->function->Unref();
  delete baton;

  if (try_catch.HasCaught()) {
    node::FatalException(try_catch);
  }
}

v8::Handle<v8::Value> Function::DoReceive(const CHND container)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;
  unsigned int parmCount;
  
  v8::Local<v8::Object> result = v8::Object::New();

  // Get resulting values for exporting/changing/table parameters
  rc = RfcGetParameterCount(this->functionDescHandle, &parmCount, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  for (unsigned int i = 0; i < parmCount; i++) {
    RFC_PARAMETER_DESC parmDesc;
    v8::Handle<v8::Value> parmValue;
    
    rc = RfcGetParameterDescByIndex(this->functionDescHandle, i, &parmDesc, &errorInfo);
    if (rc != RFC_OK) {
      return RFC_ERROR(errorInfo);
    }

    switch (parmDesc.direction) {
      case RFC_IMPORT:
        //break;
      case RFC_CHANGING:
      case RFC_TABLES:
      case RFC_EXPORT:
        parmValue = this->GetParameter(container, parmDesc);
        if (IsException(parmValue)) {
          return ThrowException(parmValue);
        }
        result->Set(v8::String::New((const uint16_t*)parmDesc.name), parmValue);
        break;
      default:
        assert(0);
        break;
    }
  }

  return scope.Close(result);
}

v8::Handle<v8::Value> Function::SetParameter(const CHND container, RFC_PARAMETER_DESC &desc, v8::Handle<v8::Value> value)
{
  v8::HandleScope scope;
  return scope.Close(this->SetValue(container, desc.type, desc.name, desc.nucLength, value));
}

v8::Handle<v8::Value> Function::SetField(const CHND container, RFC_FIELD_DESC &desc, v8::Handle<v8::Value> value)
{
  v8::HandleScope scope;
  return scope.Close(this->SetValue(container, desc.type, desc.name, desc.nucLength, value));
}

v8::Handle<v8::Value> Function::SetValue(const CHND container, RFCTYPE type, const SAP_UC *name, unsigned len, v8::Handle<v8::Value> value)
{
  v8::HandleScope scope;

  v8::Handle<v8::Value> result = v8::Undefined();

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
      assert(0);
      return RFC_ERROR("RFC type not implemented: ", v8::Uint32::New(type)->ToString());
      break;
  }

  if (IsException(result)) {
    return scope.Close(result);
  }

  return scope.Close(v8::Null());  
}

v8::Handle<v8::Value> Function::StructureToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;
  RFC_STRUCTURE_HANDLE strucHandle;

  rc = RfcGetStructure(container, name, &strucHandle, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  return scope.Close(this->StructureToExternal(container, strucHandle, value));
}

v8::Handle<v8::Value> Function::StructureToExternal(const CHND container, const RFC_STRUCTURE_HANDLE struc, v8::Handle<v8::Value> value)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;
  RFC_TYPE_DESC_HANDLE typeHandle;
  RFC_FIELD_DESC fieldDesc;
  unsigned fieldCount;

  if (!value->IsObject()) {
    return RFC_ERROR("Argument has unexpected type: ", v8::String::New((const uint16_t*)(fieldDesc.name)));
  }
  v8::Local<v8::Object> valueObj = value->ToObject();

  typeHandle = RfcDescribeType(struc, &errorInfo);
  assert(typeHandle);
  if (typeHandle == nullptr) {
    return RFC_ERROR(errorInfo);
  }

  rc = RfcGetFieldCount(typeHandle, &fieldCount, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  for (unsigned int i = 0; i < fieldCount; i++) {
    rc = RfcGetFieldDescByIndex(typeHandle, i, &fieldDesc, &errorInfo);
    if (rc != RFC_OK) {
      return RFC_ERROR(errorInfo);
    }

    v8::Local<v8::String> fieldName = v8::String::New((const uint16_t*)(fieldDesc.name));

    if (valueObj->Has(fieldName)) {
      v8::Handle<v8::Value> result = this->SetField(struc, fieldDesc, valueObj->Get(fieldName));
      // Bail out on exception
      if (IsException(result)) {
        return scope.Close(result);
      }
    }
  }

  return scope.Close(v8::Null());
}

v8::Handle<v8::Value> Function::TableToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;
  RFC_TABLE_HANDLE tableHandle;
  RFC_STRUCTURE_HANDLE strucHandle;
  uint32_t rowCount;

  if (!value->IsArray()) {
    return RFC_ERROR("Argument has unexpected type: ", v8::String::New((const uint16_t*)(name)));
  }

  rc = RfcGetTable(container, name, &tableHandle, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  v8::Handle<v8::Array> source = v8::Local<v8::Array>(v8::Array::Cast(*value));
  rowCount = source->Length();

  for (uint32_t i = 0; i < rowCount; i++){
    strucHandle = RfcAppendNewRow(tableHandle, nullptr);
    
    v8::Handle<v8::Value> line = this->StructureToExternal(container, strucHandle, source->CloneElementAt(i));
    // Bail out on exception
    if (IsException(line)) {
      return scope.Close(line);
    }
  }

  return scope.Close(v8::Null());
}

v8::Handle<v8::Value> Function::StringToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;

  if (!value->IsString()) {
    return RFC_ERROR("Argument has unexpected type: ", v8::String::New((const uint16_t*)(name)));
  }

  v8::String::Value valueU16(value->ToString());
  rc = RfcSetString(container, name, (const SAP_UC*)*valueU16, valueU16.length(), &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  return scope.Close(v8::Null());
}

v8::Handle<v8::Value> Function::XStringToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;

  if (!value->IsString()) {
    return RFC_ERROR("Argument has unexpected type: ", v8::String::New((const uint16_t*)(name)));
  }

  v8::String::AsciiValue valueAscii(value->ToString());
  rc = RfcSetXString(container, name, reinterpret_cast<SAP_RAW*>(*valueAscii), valueAscii.length(), &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  return scope.Close(v8::Null());
}

v8::Handle<v8::Value> Function::NumToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value, unsigned len)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;

  if (!value->IsString()) {
    return RFC_ERROR("Argument has unexpected type: ", v8::String::New((const uint16_t*)(name)));
  }

  v8::String::Value valueU16(value->ToString());
  if (valueU16.length() > len) {
    return RFC_ERROR("Argument exceeds maximum length: ", v8::String::New((const uint16_t*)(name)));
  }
  
  rc = RfcSetNum(container, name, (const RFC_NUM*)*valueU16, valueU16.length(), &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  return scope.Close(v8::Null());
}

v8::Handle<v8::Value> Function::CharToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value, unsigned len)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;

  if (!value->IsString()) {
    return RFC_ERROR("Argument has unexpected type: ", v8::String::New((const uint16_t*)(name)));
  }

  v8::String::Value valueU16(value->ToString());
  if (valueU16.length() > len) {
    return RFC_ERROR("Argument exceeds maximum length: ", v8::String::New((const uint16_t*)(name)));
  }

  rc = RfcSetChars(container, name, (const RFC_CHAR*)*valueU16, valueU16.length(), &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }
  
  return scope.Close(v8::Null());
}

v8::Handle<v8::Value> Function::ByteToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value, unsigned len)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;

  if (!value->IsString()) {
    return RFC_ERROR("Argument has unexpected type: ", v8::String::New((const uint16_t*)(name)));
  }

  v8::String::AsciiValue valueAscii(value->ToString());
  if (valueAscii.length() > len) {
    return RFC_ERROR("Argument exceeds maximum length: ", v8::String::New((const uint16_t*)(name)));
  }
  
  rc = RfcSetBytes(container, name, reinterpret_cast<SAP_RAW*>(*valueAscii), len, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }
  
  return scope.Close(v8::Null());
}


v8::Handle<v8::Value> Function::IntToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;

  if (!value->IsInt32()) {
    return RFC_ERROR("Argument has unexpected type: ", v8::String::New((const uint16_t*)(name)));
  }
  RFC_INT rfcValue = value->ToInt32()->Value();

  rc = RfcSetInt(container, name, rfcValue, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  return scope.Close(v8::Null());
}

v8::Handle<v8::Value> Function::Int1ToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;

  if (!value->IsInt32()) {
    return RFC_ERROR("Argument has unexpected type: ", v8::String::New((const uint16_t*)(name)));
  }
  int32_t convertedValue = value->ToInt32()->Value();
  if ((convertedValue < -128) || (convertedValue > 127)) {
    return RFC_ERROR("Argument out of range: ", v8::String::New((const uint16_t*)(name)));
  }
  RFC_INT1 rfcValue = convertedValue;

  rc = RfcSetInt1(container, name, rfcValue, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  return scope.Close(v8::Null());
}

v8::Handle<v8::Value> Function::Int2ToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;

  if (!value->IsInt32()) {
    return RFC_ERROR("Argument has unexpected type: ", v8::String::New((const uint16_t*)(name)));
  }

  int32_t convertedValue = value->ToInt32()->Value();
  if ((convertedValue < -32768) || (convertedValue > 32767)) {
    return RFC_ERROR("Argument out of range: ", v8::String::New((const uint16_t*)(name)));
  }
  RFC_INT2 rfcValue = convertedValue;

  rc = RfcSetInt2(container, name, rfcValue, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  return scope.Close(v8::Null());
}

v8::Handle<v8::Value> Function::FloatToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;

  if (!value->IsNumber()) {
    return RFC_ERROR("Argument has unexpected type: ", v8::String::New((const uint16_t*)(name)));
  }
  RFC_FLOAT rfcValue = value->ToNumber()->Value();

  rc = RfcSetFloat(container, name, rfcValue, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  return scope.Close(v8::Null());
}

v8::Handle<v8::Value> Function::DateToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value)
{
  v8::HandleScope scope;
  RFC_ERROR_INFO errorInfo;

  if (!value->IsString()) {
    return RFC_ERROR("Argument has unexpected type: ", v8::String::New((const uint16_t*)(name)));
  }

  v8::Local<v8::String> str = value->ToString();
  if (str->Length() != 8) {
    return RFC_ERROR("Invalid date format: ", v8::String::New((const uint16_t*)(name)));
  }
  
  v8::String::Value rfcValue(str);
  assert(*rfcValue);
  RFC_RC rc = RfcSetDate(container, name, (const RFC_CHAR*)*rfcValue, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  return scope.Close(v8::Null());
}

v8::Handle<v8::Value> Function::TimeToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value)
{
  v8::HandleScope scope;
  RFC_ERROR_INFO errorInfo;

  if (!value->IsString()) {
    return RFC_ERROR("Argument has unexpected type: ", v8::String::New((const uint16_t*)(name)));
  }

  v8::Local<v8::String> str = value->ToString();
  if (str->Length() != 6) {
    return RFC_ERROR("Invalid time format: ", v8::String::New((const uint16_t*)(name)));
  }

  v8::String::Value rfcValue(str);
  assert(*rfcValue);
  RFC_RC rc = RfcSetTime(container, name, (const RFC_CHAR*)*rfcValue, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  return scope.Close(v8::Null());
}

v8::Handle<v8::Value> Function::BCDToExternal(const CHND container, const SAP_UC *name, v8::Handle<v8::Value> value)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;

  if (!value->IsNumber()) {
    return RFC_ERROR("Argument has unexpected type: ", v8::String::New((const uint16_t*)(name)));
  }

  v8::String::Value valueU16(value->ToString());
    
  rc = RfcSetString(container, name, (const SAP_UC*)*valueU16, valueU16.length(), &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }
  
  return scope.Close(v8::Null());
}

v8::Handle<v8::Value> Function::GetParameter(const CHND container, const RFC_PARAMETER_DESC &desc)
{
  v8::HandleScope scope;
  return scope.Close(this->GetValue(container, desc.type, desc.name, desc.nucLength));
}

v8::Handle<v8::Value> Function::GetField(const CHND container, const RFC_FIELD_DESC &desc)
{
  v8::HandleScope scope;
  return scope.Close(this->GetValue(container, desc.type, desc.name, desc.nucLength));
}

v8::Handle<v8::Value> Function::GetValue(const CHND container, RFCTYPE type, const SAP_UC *name, unsigned len)
{
  v8::HandleScope scope;
  v8::Handle<v8::Value> value = v8::Null();

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
      assert(0);
      return RFC_ERROR("RFC type not implemented: ", v8::Uint32::New(type)->ToString());
      break;
  }

  return scope.Close(value);
}

v8::Handle<v8::Value> Function::StructureToInternal(const CHND container, const SAP_UC *name)
{
  v8::HandleScope scope;
  RFC_ERROR_INFO errorInfo;
  RFC_RC rc = RFC_OK;
  RFC_STRUCTURE_HANDLE strucHandle;

  rc = RfcGetStructure(container, name, &strucHandle, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  return scope.Close(this->StructureToInternal(container, strucHandle));
}

v8::Handle<v8::Value> Function::StructureToInternal(const CHND container, const RFC_STRUCTURE_HANDLE struc)
{
  v8::HandleScope scope;
  RFC_ERROR_INFO errorInfo;
  RFC_RC rc = RFC_OK;
  RFC_TYPE_DESC_HANDLE typeHandle;
  RFC_FIELD_DESC fieldDesc;
  unsigned fieldCount;

  typeHandle = RfcDescribeType(struc, &errorInfo);
  assert(typeHandle);
  if (typeHandle == nullptr) {
    return RFC_ERROR(errorInfo);
  }

  rc = RfcGetFieldCount(typeHandle, &fieldCount, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  v8::Local<v8::Object> obj = v8::Object::New();

  for (unsigned int i = 0; i < fieldCount; i++) {
    rc = RfcGetFieldDescByIndex(typeHandle, i, &fieldDesc, &errorInfo);
    if (rc != RFC_OK) {
      return RFC_ERROR(errorInfo);
    }

    v8::Handle<v8::Value> value = this->GetField(struc, fieldDesc);
    // Bail out on exception
    if (IsException(value)) {
      return scope.Close(value);
    }
    obj->Set(v8::String::New((const uint16_t*)(fieldDesc.name)), value);
  }

  return scope.Close(obj);
}

v8::Handle<v8::Value> Function::TableToInternal(const CHND container, const SAP_UC *name)
{
  v8::HandleScope scope;
  RFC_ERROR_INFO errorInfo;
  RFC_RC rc = RFC_OK;
  RFC_TABLE_HANDLE tableHandle;
  RFC_STRUCTURE_HANDLE strucHandle;
  unsigned rowCount;

  rc = RfcGetTable(container, name, &tableHandle, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  rc = RfcGetRowCount(tableHandle, &rowCount, nullptr);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  // Create array holding table lines
  v8::Local<v8::Array> obj = v8::Array::New();
  
  for (unsigned int i = 0; i < rowCount; i++){
    RfcMoveTo(tableHandle, i, nullptr);
    strucHandle = RfcGetCurrentRow(tableHandle, nullptr);

    v8::Handle<v8::Value> line = this->StructureToInternal(container, strucHandle);
    // Bail out on exception
    if (IsException(line)) {
      return scope.Close(line);
    }
    obj->Set(v8::Integer::New(i), line);
  }

  return scope.Close(obj);
}

v8::Handle<v8::Value> Function::StringToInternal(const CHND container, const SAP_UC *name)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;
  unsigned strLen, retStrLen;

  rc = RfcGetStringLength(container, name, &strLen, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  if (strLen == 0) {
    return scope.Close(v8::String::New(""));
  }

  SAP_UC *buffer = static_cast<SAP_UC*>(malloc((strLen + 1) * sizeof(SAP_UC)));
  assert(buffer);
  memset(buffer, 0, (strLen + 1) * sizeof(SAP_UC));
  
  rc = RfcGetString(container, name, buffer, strLen + 1, &retStrLen, &errorInfo);
  if (rc != RFC_OK) {
    free(buffer);
    return RFC_ERROR(errorInfo);
  }
  
  v8::Local<v8::String> value = v8::String::New((const uint16_t*)(buffer));

  free(buffer);
      
  return scope.Close(value);
}

v8::Handle<v8::Value> Function::XStringToInternal(const CHND container, const SAP_UC *name)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;
  unsigned strLen, retStrLen;

  rc = RfcGetStringLength(container, name, &strLen, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  if (strLen == 0) {
    return scope.Close(v8::String::New(""));
  }

  SAP_RAW *buffer = static_cast<SAP_RAW*>(malloc(strLen * sizeof(SAP_RAW)));
  assert(buffer);
  memset(buffer, 0, strLen * sizeof(SAP_RAW));
  
  rc = RfcGetXString(container, name, buffer, strLen, &retStrLen, &errorInfo);
  if (rc != RFC_OK) {
    free(buffer);
    return RFC_ERROR(errorInfo);
  }
  
  v8::Local<v8::String> value = v8::String::New(reinterpret_cast<char*>(buffer), strLen);

  free(buffer);
      
  return scope.Close(value);
}

v8::Handle<v8::Value> Function::NumToInternal(const CHND container, const SAP_UC *name, unsigned len)
{
  v8::HandleScope scope;
  RFC_ERROR_INFO errorInfo;

  RFC_NUM *buffer = static_cast<RFC_NUM*>(malloc((len + 1) * sizeof(RFC_NUM)));
  assert(buffer);
  memset(buffer, 0, (len + 1) * sizeof(RFC_NUM));
  
  RFC_RC rc = RfcGetNum(container, name, buffer, len, &errorInfo);
  if (rc != RFC_OK) {
    free(buffer);
    return RFC_ERROR(errorInfo);
  }
  
  v8::Local<v8::String> value = v8::String::New((const uint16_t*)(buffer));

  free(buffer);
      
  return scope.Close(value);
}

v8::Handle<v8::Value> Function::CharToInternal(const CHND container, const SAP_UC *name, unsigned len)
{
  v8::HandleScope scope;
  RFC_ERROR_INFO errorInfo;

  RFC_CHAR *buffer = static_cast<RFC_CHAR*>(malloc((len + 1) * sizeof(RFC_CHAR)));
  assert(buffer);
  memset(buffer, 0, (len + 1) * sizeof(RFC_CHAR));
  
  RFC_RC rc = RfcGetChars(container, name, buffer, len, &errorInfo);
  if (rc != RFC_OK) {
    free(buffer);
    return RFC_ERROR(errorInfo);
  }
  
  v8::Local<v8::String> value = v8::String::New((const uint16_t*)(buffer));

  free(buffer);
      
  return scope.Close(value);
}

v8::Handle<v8::Value> Function::ByteToInternal(const CHND container, const SAP_UC *name, unsigned len)
{
  v8::HandleScope scope;
  RFC_ERROR_INFO errorInfo;

  RFC_BYTE *buffer = static_cast<RFC_BYTE*>(malloc(len * sizeof(RFC_BYTE)));
  assert(buffer);
  memset(buffer, 0, len * sizeof(RFC_BYTE));
  
  RFC_RC rc = RfcGetBytes(container, name, buffer, len, &errorInfo);
  if (rc != RFC_OK) {
    free(buffer);
    return RFC_ERROR(errorInfo);
  }
  
  v8::Local<v8::String> value = v8::String::New(reinterpret_cast<const char*>(buffer));

  free(buffer);
      
  return scope.Close(value);
}

v8::Handle<v8::Value> Function::IntToInternal(const CHND container, const SAP_UC *name)
{
  v8::HandleScope scope;
  RFC_ERROR_INFO errorInfo;
  RFC_INT value;

  RFC_RC rc = RfcGetInt(container, name, &value, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  return scope.Close(v8::Integer::New(value));
}

v8::Handle<v8::Value> Function::Int1ToInternal(const CHND container, const SAP_UC *name)
{
  v8::HandleScope scope;
  RFC_ERROR_INFO errorInfo;
  RFC_INT1 value;

  RFC_RC rc = RfcGetInt1(container, name, &value, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  return scope.Close(v8::Integer::New(value));
}

v8::Handle<v8::Value> Function::Int2ToInternal(const CHND container, const SAP_UC *name)
{
  v8::HandleScope scope;
  RFC_ERROR_INFO errorInfo;
  RFC_INT2 value;

  RFC_RC rc = RfcGetInt2(container, name, &value, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  return scope.Close(v8::Integer::New(value));
}

v8::Handle<v8::Value> Function::FloatToInternal(const CHND container, const SAP_UC *name)
{
  v8::HandleScope scope;
  RFC_ERROR_INFO errorInfo;
  RFC_FLOAT value;

  RFC_RC rc = RfcGetFloat(container, name, &value, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  return scope.Close(v8::Number::New(value));
}

v8::Handle<v8::Value> Function::DateToInternal(const CHND container, const SAP_UC *name)
{
  v8::HandleScope scope;
  RFC_ERROR_INFO errorInfo;
  RFC_DATE date = { 0 };

  RFC_RC rc = RfcGetDate(container, name, date, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  assert(sizeof(RFC_CHAR) > 0); // Shouldn't occur except in case of a compiler glitch
  v8::Local<v8::String> value = v8::String::New((const uint16_t*)(date), sizeof(RFC_DATE) / sizeof(RFC_CHAR));

  return scope.Close(value);
}

v8::Handle<v8::Value> Function::TimeToInternal(const CHND container, const SAP_UC *name)
{
  v8::HandleScope scope;
  RFC_ERROR_INFO errorInfo;
  RFC_TIME time = { 0 };

  RFC_RC rc = RfcGetTime(container, name, time, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }

  assert(sizeof(RFC_CHAR) > 0); // Shouldn't occur except in case of a compiler glitch
  v8::Local<v8::String> value = v8::String::New((const uint16_t*)(time), sizeof(RFC_TIME) / sizeof(RFC_CHAR));
      
  return scope.Close(value);
}

v8::Handle<v8::Value> Function::BCDToInternal(const CHND container, const SAP_UC *name)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;
  unsigned strLen = 25;
  unsigned retStrLen;
  SAP_UC *buffer;

  do {
    buffer = static_cast<SAP_UC*>(malloc((strLen + 1) * sizeof(SAP_UC)));
    assert(buffer);
    memset(buffer, 0, (strLen + 1) * sizeof(SAP_UC));
  
    rc = RfcGetString(container, name, buffer, strLen + 1, &retStrLen, &errorInfo);

    if (rc == RFC_BUFFER_TOO_SMALL) {
      // Retry with suggested string length
      free(buffer);
      strLen = retStrLen;
    } else if (rc != RFC_OK) {
      return RFC_ERROR(errorInfo);
    }
  } while (rc == RFC_BUFFER_TOO_SMALL);
  
  v8::Local<v8::String> value = v8::String::New((const uint16_t*)(buffer), retStrLen);

  free(buffer);
      
  return scope.Close(value->ToNumber());
}
