/*
-----------------------------------------------------------------------------
Copyright (c) 2016 Scheer E2E AG

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

#include "Loggable.h"
#include <algorithm>

const std::string Loggable::Levels::SILLY	= "silly";
const std::string Loggable::Levels::DBG		= "debug";
const std::string Loggable::Levels::VERBOSE	= "verbose";
const std::string Loggable::Levels::INFO	= "info";
const std::string Loggable::Levels::WARN	= "warn";
const std::string Loggable::Levels::ERR	= "error";

const std::string Loggable::API_CALL_MESSAGE	= "SAPNWRFC API Call";

v8::Local<v8::Value> metaToJS( const Loggable::LogEntry::Meta& meta)
{
  Nan::EscapableHandleScope scope;

  v8::Local<v8::Object> result = Nan::New<v8::Object>();

  for( Loggable::LogEntry::Meta::const_iterator it = meta.begin(), eit = meta.end(); it != eit; ++it)
  {
    result->Set(
		Nan::New<v8::String>(it->first).ToLocalChecked(),
		Nan::New<v8::String>(it->second).ToLocalChecked()
    );
  }

  return scope.Escape(result);
}

static v8::Local<v8::Value> errorInfoToJS(const RFC_ERROR_INFO &info)
{
  Nan::EscapableHandleScope scope;

  v8::Local<v8::Object> obj = Nan::New<v8::Object>();
  obj->Set(
      Nan::New<v8::String>("code").ToLocalChecked(),
      Nan::New<v8::Integer>(info.code)
  );
  obj->Set(
      Nan::New<v8::String>("group").ToLocalChecked(),
      Nan::New<v8::Integer>(info.group)
  );
  obj->Set(
      Nan::New<v8::String>("key").ToLocalChecked(),
      Nan::New<v8::String>((const uint16_t*)(info.key)).ToLocalChecked()
  );
  obj->Set(
      Nan::New<v8::String>("class").ToLocalChecked(),
      Nan::New<v8::String>((const uint16_t*)(info.abapMsgClass)).ToLocalChecked()
  );
  obj->Set(
      Nan::New<v8::String>("type").ToLocalChecked(),
      Nan::New<v8::String>((const uint16_t*)(info.abapMsgType)).ToLocalChecked()
  );
  obj->Set(
      Nan::New<v8::String>("number").ToLocalChecked(),
      Nan::New<v8::String>((const uint16_t*)(info.abapMsgNumber)).ToLocalChecked()
  );
  obj->Set(
      Nan::New<v8::String>("msgv1").ToLocalChecked(),
      Nan::New<v8::String>((const uint16_t*)(info.abapMsgV1)).ToLocalChecked()
  );
  obj->Set(
      Nan::New<v8::String>("msgv2").ToLocalChecked(),
      Nan::New<v8::String>((const uint16_t*)(info.abapMsgV2)).ToLocalChecked()
  );
  obj->Set(
      Nan::New<v8::String>("msgv3").ToLocalChecked(),
      Nan::New<v8::String>((const uint16_t*)(info.abapMsgV3)).ToLocalChecked()
  );
  obj->Set(
      Nan::New<v8::String>("msgv4").ToLocalChecked(),
      Nan::New<v8::String>((const uint16_t*)(info.abapMsgV4)).ToLocalChecked()
  );

  return scope.Escape(obj);
}

void Loggable::logFunctionWeakCallback(const Nan::WeakCallbackInfo<Loggable>& data)
{
  data.GetParameter()->resetLogFunction();
}

Loggable::~Loggable()
{
  resetLogFunction();
}

void Loggable::log(const std::string& level, const std::string& message)
{
  log(level, Nan::New<v8::String>(message).ToLocalChecked());
}

void Loggable::log(const std::string& level, v8::Local<v8::Value> message)
{
  log(level, message, Nan::Undefined());
}

void Loggable::log(const std::string& level, const std::string& message, v8::Local<v8::Value> meta)
{
  log(level, Nan::New<v8::String>(message).ToLocalChecked(), meta);
}

void Loggable::log(const std::string& level, v8::Local<v8::Value> message, v8::Local<v8::Value> meta)
{
  logDeferred();
  log_(level, message, meta);
}

void Loggable::log_(const std::string& level, v8::Local<v8::Value> message, v8::Local<v8::Value> meta)
{
  if( logFunction.IsEmpty()) {
    logFunction =
        Nan::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>>(
          v8::Local<v8::Function>::Cast(
	                Nan::Get(handle(),
	                         Nan::New<v8::String>("_log").ToLocalChecked()).ToLocalChecked()));
    logFunction.SetWeak(this, logFunctionWeakCallback, Nan::WeakCallbackType::kParameter);
    logFunction.MarkIndependent();
  }

  v8::Local<v8::Value> argv[3];
  argv[0] = Nan::New<v8::String>(level).ToLocalChecked();
  argv[1] = message->ToString();
  argv[2] = meta;
  assert(!logFunction.IsEmpty());
  Nan::Call(Nan::New(logFunction), handle(), 3, argv);
}

void Loggable::log(const LogEntry& logEntry)
{
  Nan::HandleScope scope;

  v8::Handle<v8::Object> meta;
  if(!logEntry.call.empty()) {
    meta = v8::Handle<v8::Object>::Cast( metaToJS( logEntry.meta));
    meta->Set(Nan::New<v8::String>("call").ToLocalChecked(), Nan::New<v8::String>(logEntry.call).ToLocalChecked());
    meta->Set(Nan::New<v8::String>("errorInfo").ToLocalChecked(), errorInfoToJS(logEntry.errorInfo));
  } else {
    meta = Nan::New<v8::Object>();
  }

  if(!logEntry.file.empty()) {
    meta->Set(Nan::New<v8::String>("file").ToLocalChecked(), Nan::New<v8::String>(logEntry.file).ToLocalChecked());
  }

  if(!logEntry.function.empty()) {
    meta->Set(Nan::New<v8::String>("function").ToLocalChecked(), Nan::New<v8::String>(logEntry.function).ToLocalChecked());
  }

  if(logEntry.line) {
    meta->Set(Nan::New<v8::String>("line").ToLocalChecked(), Nan::New<v8::Integer>(logEntry.line));
  }

  log_(logEntry.level, Nan::New<v8::String>(logEntry.message).ToLocalChecked(), meta);
}

void Loggable::deferLog(const std::string& level, const std::string& message, const Loggable::LogEntry::Meta& meta)
{
  LogEntry entry;
  entry.level = level;
  entry.message = message;
  entry.meta = meta;
  deferredLogs.push_back( entry);
}

void Loggable::createAPILogEntry_(Loggable::LogEntry& logEntry, const std::string& call,
                                  const std::string& file, const std::string& function,
                                  unsigned long line, RFC_ERROR_INFO& errorInfo,
                                  const Loggable::LogEntry::Meta& meta)
{
  logEntry.level = Levels::DBG;
  logEntry.message = API_CALL_MESSAGE;
  logEntry.call = call;
  logEntry.file = file;
  logEntry.function = function;
  logEntry.line = line;
  logEntry.errorInfo = errorInfo;
  logEntry.meta = meta;
}

void Loggable::logAPICall(const std::string& call, const std::string& file, const std::string& function,
                          unsigned long line, RFC_ERROR_INFO& errorInfo)
{
  logAPICall(call, file, function, line, errorInfo, LogEntry::Meta());
}

void Loggable::logAPICall(const std::string& call, const std::string& file, const std::string& function,
                          unsigned long line, RFC_ERROR_INFO& errorInfo, const Loggable::LogEntry::Meta& meta)
{
  logDeferred();
  LogEntry logEntry;
  createAPILogEntry_(logEntry, call, file, function, line, errorInfo, meta);
  log(logEntry);
}

void Loggable::deferLogAPICall(const std::string& call, const std::string& file, const std::string& function,
                               unsigned long line, RFC_ERROR_INFO& errorInfo)
{
  deferLogAPICall(call, file, function, line, errorInfo, LogEntry::Meta());
}

void Loggable::deferLogAPICall(const std::string& call, const std::string& file, const std::string& function,
                               unsigned long line, RFC_ERROR_INFO& errorInfo, const Loggable::LogEntry::Meta& meta)
{
  LogEntry logEntry;
  createAPILogEntry_(logEntry, call, file, function, line, errorInfo, meta);
  deferredLogs.push_back( logEntry);
}

void Loggable::logDeferred()
{
  for(LogEntries::iterator it = deferredLogs.begin(), eit = deferredLogs.end(); it != eit; ++it)
  {
    log(*it);
  }
  deferredLogs.clear();
}

void Loggable::resetLogFunction()
{
  logFunction.Reset();
}
