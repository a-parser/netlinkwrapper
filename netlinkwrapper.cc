#include <nan.h>
#include "netlinkwrapper.h"
#include "netlink/exception.h"

using namespace v8;

Persistent<Function> NetLinkWrapper::constructor;

NetLinkWrapper::NetLinkWrapper()
{
}

NetLinkWrapper::~NetLinkWrapper()
{
    if (this->socket != nullptr)
    {
        delete this->socket;
    }
}

void NetLinkWrapper::Init(Local<Object> exports)
{
    Isolate* isolate = Isolate::GetCurrent();

    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    tpl->SetClassName(String::NewFromUtf8(isolate, "NetLinkWrapper").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    // Prototype
    NODE_SET_PROTOTYPE_METHOD(tpl, "connect", Connect);
    NODE_SET_PROTOTYPE_METHOD(tpl, "blocking", Blocking);
    NODE_SET_PROTOTYPE_METHOD(tpl, "read", Read);
    NODE_SET_PROTOTYPE_METHOD(tpl, "write", Write);
    NODE_SET_PROTOTYPE_METHOD(tpl, "disconnect", Disconnect);

    constructor.Reset(isolate, Nan::GetFunction(tpl).ToLocalChecked());
    Nan::Set(exports, Nan::New<String>("NetLinkWrapper").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

void NetLinkWrapper::New(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    if (args.IsConstructCall())
    {
        // Invoked as constructor: `new NetLinkWrapper(...)`
        NetLinkWrapper* obj = new NetLinkWrapper();
        obj->Wrap(args.This());
        args.GetReturnValue().Set(args.This());
    }
    else
    {
        // Invoked as plain function `NetLinkWrapper(...)`, turn into construct call.
        const int argc = 1;
        Local<Value> argv[argc] = { args[0] };
        Local<Function> cons = Local<Function>::New(isolate, constructor);
        args.GetReturnValue().Set(Nan::NewInstance(cons, argc, argv).ToLocalChecked());
    }
}

void NetLinkWrapper::Connect(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    NetLinkWrapper* obj = ObjectWrap::Unwrap<NetLinkWrapper>(args.Holder());

    if(args.Length() != 2 && args.Length() != 1)
    {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "'connect' requires the arguments port and optionally host").ToLocalChecked()));
        return;
    }

    if(!args[0]->IsNumber())
    {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "'connect' second arg should be number for port on server").ToLocalChecked()));
        return;
    }
    int port = (int)args[0]->NumberValue(isolate->GetCurrentContext()).FromJust();

    std::string server = "127.0.0.1";
    if(args[1]->IsString())
    {
        Nan::Utf8String param1(args[1]);
        server = std::string(*param1);
    }

    try
    {
        obj->socket = new NL::Socket(server, port);
    }
    catch(NL::Exception& e)
    {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, e.what()).ToLocalChecked()));
        return;
    }
}

void NetLinkWrapper::Blocking(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    NetLinkWrapper* obj = ObjectWrap::Unwrap<NetLinkWrapper>(args.Holder());

    if(args.Length() == 0)
    {
        bool blocking = false;
        try
        {
            blocking = obj->socket->blocking();
        }
        catch(NL::Exception& e)
        {
            isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, e.what()).ToLocalChecked()));
            return;
        }
        args.GetReturnValue().Set(Boolean::New(isolate, blocking));
    }
    else if(args.Length() == 1)
    {
        if(!args[0]->IsBoolean())
        {
            isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "first optional arg when passed must be boolean to set blocking to").ToLocalChecked()));
            return;
        }

        try
        {
            obj->socket->blocking(args[0]->BooleanValue(args.GetIsolate()));
        }
        catch(NL::Exception& e)
        {
            isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, e.what()).ToLocalChecked()));
            return;
        }
    }
    else
    {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "too many args sent to blocking").ToLocalChecked()));
        return;
    }
}

void NetLinkWrapper::Read(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    NetLinkWrapper* obj = ObjectWrap::Unwrap<NetLinkWrapper>(args.Holder());

    if((args.Length() != 1 && args.Length() != 2) || !args[0]->IsNumber())
    {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "'read' first argument must be a number representing how many bytes to try to read").ToLocalChecked()));
        return;
    }
    size_t bufferSize = (int)args[0]->NumberValue(isolate->GetCurrentContext()).FromJust();
    char* buffer = new char[bufferSize];

    if(args.Length() == 2 && args[0]->IsBoolean()) {
        try
        {
            obj->socket->blocking(args[1]->BooleanValue(args.GetIsolate()));
        }
        catch(NL::Exception& e)
        {
            isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, e.what()).ToLocalChecked()));
            return;
        }
    }

    int bufferRead = 0;
    try
    {
        bufferRead = obj->socket->read(buffer, bufferSize);
    }
    catch(NL::Exception& e)
    {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, e.what()).ToLocalChecked()));
        return;
    }

    if(bufferRead > -1 && bufferRead <= (int)bufferSize) // range check
    {
        std::string read(buffer, bufferRead);
        args.GetReturnValue().Set(v8::String::NewFromUtf8(isolate, read.c_str()).ToLocalChecked());
    }
    //else it did not read any data, so this will return undefined

    delete[] buffer;
}

void NetLinkWrapper::Write(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    NetLinkWrapper* obj = ObjectWrap::Unwrap<NetLinkWrapper>(args.Holder());

    if(args.Length() != 1 || !args[0]->IsString())
    {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "'send' first argument must be a string to send").ToLocalChecked()));
        return;
    }
    Nan::Utf8String param1(args[0]);
    std::string writing(*param1);

    try
    {
        obj->socket->send(writing.c_str(), writing.length());
    }
    catch(NL::Exception& e)
    {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, e.what()).ToLocalChecked()));
        return;
    }
}

void NetLinkWrapper::Disconnect(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    NetLinkWrapper* obj = ObjectWrap::Unwrap<NetLinkWrapper>(args.Holder());

    try
    {
        obj->socket->disconnect();
    }
    catch(NL::Exception& e)
    {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, e.what()).ToLocalChecked()));
        return;
    }
}
