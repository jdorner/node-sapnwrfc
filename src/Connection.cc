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

#include "Common.h"
#include "Connection.h"
#include "Function.h"

v8::Persistent<v8::FunctionTemplate> Connection::ctorTemplate;

Connection::Connection() :
  loginParamsSize(0),
  loginParams(nullptr),
  connectionHandle(nullptr)
{
  uv_mutex_init(&this->invocationMutex);
}

Connection::~Connection()
{
  this->CloseConnection();

  uv_mutex_destroy(&this->invocationMutex);
  
  for (unsigned int i = 0; i < this->loginParamsSize; i++) {
     free(const_cast<SAP_UC*>(loginParams[i].name));
     free(const_cast<SAP_UC*>(loginParams[i].value));
  }
  free(loginParams);
  
  if (!this->cbOpen.IsEmpty()) {
    this->cbOpen.Dispose();
    this->cbOpen.Clear();
  }
}

v8::Handle<v8::Value> Connection::New (const v8::Arguments& args)
{
  v8::HandleScope scope;

  if (!args.IsConstructCall()) {
    return THROW_V8_EXCEPTION("Invalid call format. Please use the 'new' operator.");
  }

  Connection *self = new Connection();
  self->Wrap(args.This());

  return args.This();
}

void Connection::Init(v8::Handle<v8::Object> target)
{
  v8::HandleScope scope;
  
  ctorTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(New));
  ctorTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  ctorTemplate->SetClassName(v8::String::NewSymbol("Connection"));

  NODE_SET_PROTOTYPE_METHOD(ctorTemplate, "GetVersion", Connection::GetVersion);
  NODE_SET_PROTOTYPE_METHOD(ctorTemplate, "Open", Connection::Open);
  NODE_SET_PROTOTYPE_METHOD(ctorTemplate, "Close", Connection::Close);
  NODE_SET_PROTOTYPE_METHOD(ctorTemplate, "Ping", Connection::Ping);
  NODE_SET_PROTOTYPE_METHOD(ctorTemplate, "Lookup", Connection::Lookup);

  target->Set(v8::String::NewSymbol("Connection"), ctorTemplate->GetFunction());
}

/**
 * @return Array
 */
v8::Handle<v8::Value> Connection::GetVersion(const v8::Arguments &args)
{
  v8::HandleScope scope;
  unsigned majorVersion, minorVersion, patchLevel;

  RfcGetVersion(&majorVersion, &minorVersion, &patchLevel);
  v8::Local<v8::Array> info = v8::Array::New(3);

  info->Set(v8::Integer::New(0), v8::Integer::New(majorVersion));
  info->Set(v8::Integer::New(1), v8::Integer::New(minorVersion));
  info->Set(v8::Integer::New(2), v8::Integer::New(patchLevel));

  return scope.Close(info);
}

v8::Handle<v8::Value> Connection::Open(const v8::Arguments &args)
{
  v8::HandleScope scope;
  Connection *self = node::ObjectWrap::Unwrap<Connection>(args.This());
  
  if (args.Length() < 2) {
    return THROW_V8_EXCEPTION("Function requires 2 arguments");
  }
  if (!args[0]->IsObject()) {
    return THROW_V8_EXCEPTION("Argument 1 must be an object");
  }
  if (!args[1]->IsFunction()) {
    return THROW_V8_EXCEPTION("Argument 2 must be a function");
  }

  v8::Local<v8::Object> optionsObj = args[0]->ToObject();
  v8::Local<v8::Array> props = optionsObj->GetPropertyNames();

  self->loginParamsSize = props->Length();
  self->loginParams = static_cast<RFC_CONNECTION_PARAMETER*>(malloc(self->loginParamsSize * sizeof(RFC_CONNECTION_PARAMETER)));
  memset(self->loginParams, 0, self->loginParamsSize * sizeof(RFC_CONNECTION_PARAMETER));
  memset(&self->errorInfo, 0, sizeof(RFC_ERROR_INFO));

  for (unsigned int i = 0; i < self->loginParamsSize; i++) {
    v8::Local<v8::Value> name = props->Get(i);
    v8::Local<v8::Value> value = optionsObj->Get(name->ToString());
    
    self->loginParams[i].name = convertToSAPUC(name);
    self->loginParams[i].value = convertToSAPUC(value);

#ifndef NDEBUG
    std::cout << convertToString(name) << "--> " << convertToString(value) << std::endl;
#endif
  }

  // Store callback
  self->cbOpen = v8::Persistent<v8::Function>::New(v8::Local<v8::Function>::Cast(args[1]));
  self->Ref();
  
  uv_work_t* req = new uv_work_t();
  req->data = self;
  uv_queue_work(uv_default_loop(), req, EIO_Open, (uv_after_work_cb)EIO_AfterOpen);
#if !NODE_VERSION_AT_LEAST(0, 7, 9)
    uv_ref(uv_default_loop());
#endif
  
  return scope.Close(v8::Undefined());
}

void Connection::EIO_Open(uv_work_t *req)
{
  Connection *self = static_cast<Connection*>(req->data);
  
  self->connectionHandle = RfcOpenConnection(self->loginParams, self->loginParamsSize, &self->errorInfo);
}

void Connection::EIO_AfterOpen(uv_work_t *req)
{
  v8::HandleScope scope;
  RFC_ERROR_INFO errorInfo;
  int isValid;
  Connection *self = static_cast<Connection*>(req->data);

  v8::Local<v8::Value> argv[1];
  argv[0] = v8::Local<v8::Value>::New(v8::Null());
  
  if (self->connectionHandle == nullptr) {
    argv[0] = v8::Local<v8::Value>::New(RfcError(self->errorInfo));
  } else {
    RfcIsConnectionHandleValid(self->connectionHandle, &isValid, &errorInfo);  
    if (!isValid) {
      argv[0] = v8::Local<v8::Value>::New(RfcError(errorInfo));
    }
  }
  
  v8::TryCatch try_catch;

  assert(!self->cbOpen.IsEmpty());
  self->cbOpen->Call(v8::Context::GetCurrent()->Global(), 1, argv);
  self->cbOpen.Dispose();
  self->cbOpen.Clear();  
  self->Unref();

  if (try_catch.HasCaught()) {
    node::FatalException(try_catch);
  }
}

v8::Handle<v8::Value> Connection::Close(const v8::Arguments& args)
{
  v8::HandleScope scope;

  Connection *self = node::ObjectWrap::Unwrap<Connection>(args.This());
  return scope.Close(self->CloseConnection());
}

v8::Handle<v8::Value> Connection::CloseConnection(void)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;

  if (this->connectionHandle != nullptr) {
    rc = RfcCloseConnection(this->connectionHandle, &errorInfo);
    if (rc != RFC_OK) {
      return RFC_ERROR(errorInfo);
    }
  }

  return scope.Close(v8::True());
}

RFC_CONNECTION_HANDLE Connection::GetConnectionHandle(void)
{
  return this->connectionHandle;
}

void Connection::LockMutex(void)
{
  uv_mutex_lock(&this->invocationMutex);
}

void Connection::UnlockMutex(void)
{
  uv_mutex_unlock(&this->invocationMutex);
}

v8::Handle<v8::Value> Connection::IsOpen(const v8::Arguments &args)
{
  v8::HandleScope scope;
  Connection *self = node::ObjectWrap::Unwrap<Connection>(args.This());
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;
  int isValid;
  
  rc = RfcIsConnectionHandleValid(self->connectionHandle, &isValid, &errorInfo);
  if (!isValid) {
    return scope.Close(v8::False());
  } else {
    return scope.Close(v8::True());
  }
}

/**
 * 
 * @return true if successful, else: RfcException
 */
v8::Handle<v8::Value> Connection::Ping(const v8::Arguments &args)
{
  v8::HandleScope scope;
  Connection *self = node::ObjectWrap::Unwrap<Connection>(args.This());
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;
  
  if (args.Length() > 0) {
    return THROW_V8_EXCEPTION("No arguments expected");
  }

  rc = RfcPing(self->connectionHandle, &errorInfo);
  if (rc != RFC_OK) {
    return RFC_ERROR(errorInfo);
  }
  
  return scope.Close(v8::True());
}

/**
 * 
 * @return Connection
 */
v8::Handle<v8::Value> Connection::Lookup(const v8::Arguments &args)
{
  v8::HandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;
  int isValid;
  
  Connection *self = node::ObjectWrap::Unwrap<Connection>(args.This());
  
  if (args.Length() != 1) {
    return THROW_V8_EXCEPTION("Function requires 1 argument");
  }
  if (!args[0]->IsString()) {
    return THROW_V8_EXCEPTION("Argument 1 must be function module name");
  }

  rc = RfcIsConnectionHandleValid(self->connectionHandle, &isValid, &errorInfo);
  if (!isValid) {
    return v8::ThrowException(RfcError(errorInfo));
  }

  v8::Local<v8::Value> f = v8::Local<v8::Value>::New(Function::NewInstance(*self, args));
  if (IsException(f)) {
    return scope.Close(v8::ThrowException(f));
  }
  
  return scope.Close(f);
}
