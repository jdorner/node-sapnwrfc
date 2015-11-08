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

  delete this->cbOpen;
  this->cbOpen = nullptr;
}

NAN_METHOD(Connection::New)
{
  if (!info.IsConstructCall()) {
    Nan::ThrowError("Invalid call format. Please use the 'new' operator.");
    return;
  }

  Connection *self = new Connection();
  self->Wrap(info.This());

  info.GetReturnValue().Set(info.This());
}

NAN_MODULE_INIT(Connection::Init)
{
  Nan::HandleScope scope;

  v8::Local<v8::FunctionTemplate> ctorTemplate = Nan::New<v8::FunctionTemplate>(New);
  ctorTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  ctorTemplate->SetClassName(Nan::New("Connection").ToLocalChecked());

  Nan::SetPrototypeMethod(ctorTemplate, "GetVersion", Connection::GetVersion);
  Nan::SetPrototypeMethod(ctorTemplate, "Open", Connection::Open);
  Nan::SetPrototypeMethod(ctorTemplate, "Close", Connection::Close);
  Nan::SetPrototypeMethod(ctorTemplate, "Ping", Connection::Ping);
  Nan::SetPrototypeMethod(ctorTemplate, "IsOpen", Connection::IsOpen);
  Nan::SetPrototypeMethod(ctorTemplate, "Lookup", Connection::Lookup);
  Nan::SetPrototypeMethod(ctorTemplate, "SetIniPath", Connection::SetIniPath);

  Nan::Set(target, Nan::New("Connection").ToLocalChecked(), ctorTemplate->GetFunction());
}

/**
 * @return Array
 */
NAN_METHOD(Connection::GetVersion)
{
  unsigned majorVersion, minorVersion, patchLevel;

  RfcGetVersion(&majorVersion, &minorVersion, &patchLevel);
  v8::Local<v8::Array> versionInfo = Nan::New<v8::Array>(3);

  versionInfo->Set(0, Nan::New<v8::Integer>(majorVersion));
  versionInfo->Set(1, Nan::New<v8::Integer>(minorVersion));
  versionInfo->Set(2, Nan::New<v8::Integer>(patchLevel));

  info.GetReturnValue().Set(versionInfo);
}

NAN_METHOD(Connection::Open)
{
  Connection *self = node::ObjectWrap::Unwrap<Connection>(info.This());

  if (info.Length() < 2) {
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

  v8::Local<v8::Object> optionsObj = info[0]->ToObject();
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
  self->cbOpen = new Nan::Callback(v8::Local<v8::Function>::Cast(info[1]));
  self->Ref();

  uv_work_t* req = new uv_work_t();
  req->data = self;
  uv_queue_work(uv_default_loop(), req, EIO_Open, (uv_after_work_cb)EIO_AfterOpen);
#if !NODE_VERSION_AT_LEAST(0, 7, 9)
    uv_ref(uv_default_loop());
#endif
}

void Connection::EIO_Open(uv_work_t *req)
{
  Connection *self = static_cast<Connection*>(req->data);

  self->connectionHandle = RfcOpenConnection(self->loginParams, self->loginParamsSize, &self->errorInfo);
}

void Connection::EIO_AfterOpen(uv_work_t *req)
{
  Nan::HandleScope scope;
  RFC_ERROR_INFO errorInfo;
  int isValid;
  Connection *self = static_cast<Connection*>(req->data);

  v8::Local<v8::Value> argv[1];
  argv[0] = Nan::Null();

  if (self->connectionHandle == nullptr) {
    argv[0] = RfcError(self->errorInfo);
  } else {
    RfcIsConnectionHandleValid(self->connectionHandle, &isValid, &errorInfo);
    if (!isValid) {
      argv[0] = RfcError(errorInfo);
    }
  }

  Nan::TryCatch try_catch;

  assert(!self->cbOpen->IsEmpty());
  self->cbOpen->Call(1, argv);
  delete self->cbOpen;
  self->cbOpen = nullptr;
  self->Unref();

  if (try_catch.HasCaught()) {
    Nan::FatalException(try_catch);
  }
}

NAN_METHOD(Connection::Close)
{
  Connection *self = node::ObjectWrap::Unwrap<Connection>(info.This());
  info.GetReturnValue().Set(self->CloseConnection());
}

v8::Local<v8::Value> Connection::CloseConnection(void)
{
  Nan::EscapableHandleScope scope;
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;

  if (this->connectionHandle != nullptr) {
    rc = RfcCloseConnection(this->connectionHandle, &errorInfo);
    if (rc != RFC_OK) {
      scope.Escape(RfcError(errorInfo));
    }
  }

  return scope.Escape(Nan::True());
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

NAN_METHOD(Connection::IsOpen)
{
  Connection *self = node::ObjectWrap::Unwrap<Connection>(info.This());
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;
  int isValid;

  rc = RfcIsConnectionHandleValid(self->connectionHandle, &isValid, &errorInfo);
  info.GetReturnValue().Set(isValid ? Nan::True() : Nan::False());
}

/**
 *
 * @return true if successful, else: RfcException
 */
NAN_METHOD(Connection::Ping)
{
  Connection *self = node::ObjectWrap::Unwrap<Connection>(info.This());
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;

  if (info.Length() > 0) {
    Nan::ThrowError("No arguments expected");
    return;
  }

  rc = RfcPing(self->connectionHandle, &errorInfo);
  if (rc != RFC_OK) {
    RETURN_RFC_ERROR(errorInfo);
  }

  info.GetReturnValue().Set(Nan::True());
}

/**
 *
 * @return Function
 */
NAN_METHOD(Connection::Lookup)
{
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;
  int isValid;

  Connection *self = node::ObjectWrap::Unwrap<Connection>(info.This());

  if (info.Length() != 1) {
    Nan::ThrowError("Function expects 1 argument");
    return;
  }
  if (!info[0]->IsString()) {
    Nan::ThrowError("Argument 1 must be function module name");
    return;
  }

  rc = RfcIsConnectionHandleValid(self->connectionHandle, &isValid, &errorInfo);
  if (!isValid) {
    Nan::ThrowError(RfcError(errorInfo));
    return;
  }

  v8::Local<v8::Value> f = Function::NewInstance(*self, info);
  info.GetReturnValue().Set(f);
}

/**
 *
 * @return true if successful, else: RfcException
 */
NAN_METHOD(Connection::SetIniPath)
{
  RFC_RC rc = RFC_OK;
  RFC_ERROR_INFO errorInfo;
  
  Connection *self = node::ObjectWrap::Unwrap<Connection>(info.This());

  if (info.Length() != 1) {
    Nan::ThrowError("Function expects 1 argument");
    return;
  }
  if (!info[0]->IsString()) {
    Nan::ThrowError("Argument 1 must be a path name");
    return;
  }

  v8::Local<v8::Value> iniPath = info[0]->ToString();

  rc = RfcSetIniPath(convertToSAPUC(iniPath), &errorInfo);
  if (rc) {
    Nan::ThrowError(RfcError(errorInfo));
    return;
  }

  info.GetReturnValue().Set(Nan::True());
}
