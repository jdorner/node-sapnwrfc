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
  
    static void Init(v8::Handle<v8::Object> target);

  protected:

    Connection();
    ~Connection();
    static v8::Handle<v8::Value> GetVersion(const v8::Arguments &args);
    static v8::Handle<v8::Value> New (const v8::Arguments& args);
    static v8::Handle<v8::Value> Open(const v8::Arguments& args);
    static v8::Handle<v8::Value> Close(const v8::Arguments& args);
    static v8::Handle<v8::Value> Ping(const v8::Arguments& args);
    static v8::Handle<v8::Value> Lookup(const v8::Arguments &args);
    static v8::Handle<v8::Value> IsOpen(const v8::Arguments &args);

    static void EIO_Open(uv_work_t *req);
    static void EIO_AfterOpen(uv_work_t *req);
    
    v8::Handle<v8::Value> CloseConnection(void);
    
    RFC_CONNECTION_HANDLE GetConnectionHandle(void);
    void LockMutex(void);
    void UnlockMutex(void);
    
    static v8::Persistent<v8::FunctionTemplate> ctorTemplate;

    unsigned int loginParamsSize;
    RFC_CONNECTION_PARAMETER *loginParams;
    RFC_ERROR_INFO errorInfo;
    RFC_CONNECTION_HANDLE connectionHandle;
    v8::Persistent<v8::Function> cbOpen;
    uv_mutex_t invocationMutex;
};

#endif /* CONNECTION_H_ */
