// filepath: c:\codedev\auricleinc\oscmex\src\ServerImpl.h
#pragma once

#include "Types.h"
#include <string>
#include <memory>
#include <mutex>
#include <map>
#include <chrono>
#include <functional>

namespace osc
{

    class Message;
    class Bundle;

    using MethodId = int;
    using MethodHandler = std::function<void(const Message&)>;
    using BundleStartHandler = std::function<void(const Bundle&)>;
    using BundleEndHandler = std::function<void(const Bundle&)>;
    using ErrorHandler = std::function<void(int, const std::string&, const std::string&)>;

    class ServerImpl
    {
    public:
        ServerImpl(std::string port, Protocol protocol);
        ~ServerImpl();

        MethodId addMethod(const std::string &pathPattern, const std::string &typeSpec, MethodHandler handler);
        MethodId addDefaultMethod(MethodHandler handler);
        bool removeMethod(MethodId id);
        void setBundleHandlers(BundleStartHandler startHandler, BundleEndHandler endHandler);
        void setErrorHandler(ErrorHandler handler);
        bool wait(std::chrono::milliseconds timeout);
        bool receive(std::chrono::milliseconds timeout);
        bool hasPendingMessages() const;
        int port() const;
        std::string url() const;

    private:
        void cleanup();
        bool initializeSocket();
        bool resolveAddress();
        void dispatchMessage(const Message &message);
        void dispatchBundle(const Bundle &bundle);
        bool matchMethod(const std::string &path, const std::string &types, std::vector<Method *> &matchedMethods);

        std::string port_;
        Protocol protocol_;
        SOCKET_TYPE socket_;
        std::map<MethodId, Method> methods_;
        MethodId nextMethodId_;
        std::mutex methodMutex_;
        BundleStartHandler bundleStartHandler_;
        BundleEndHandler bundleEndHandler_;
        ErrorHandler errorHandler_;
        bool networkingInitialized;
    };

} // namespace osc