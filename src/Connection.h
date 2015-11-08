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

#ifndef CONNECTION_H_
#define CONNECTION_H_

#include "Common.h"
#include <v8.h>
#include <node.h>
#include <node_version.h>
#include <uv.h>
#include <sapnwrfc.h>
#include <iostream>

class Connection : public node::ObjectWrap
{
  friend class Function;

  public:

    static NAN_MODULE_INIT(Init);

  protected:

    Connection();
    ~Connection();
    static NAN_METHOD(GetVersion);
    static NAN_METHOD(New);
    static NAN_METHOD(Open);
    static NAN_METHOD(Close);
    static NAN_METHOD(Ping);
    static NAN_METHOD(Lookup);
    static NAN_METHOD(IsOpen);
    static NAN_METHOD(SetIniPath);

    static void EIO_Open(uv_work_t *req);
    static void EIO_AfterOpen(uv_work_t *req);

    v8::Local<v8::Value> CloseConnection(void);

    RFC_CONNECTION_HANDLE GetConnectionHandle(void);
    void LockMutex(void);
    void UnlockMutex(void);

    unsigned int loginParamsSize;
    RFC_CONNECTION_PARAMETER *loginParams;
    RFC_ERROR_INFO errorInfo;
    RFC_CONNECTION_HANDLE connectionHandle;
    Nan::Callback *cbOpen;

    uv_mutex_t invocationMutex;
};

#endif /* CONNECTION_H_ */
