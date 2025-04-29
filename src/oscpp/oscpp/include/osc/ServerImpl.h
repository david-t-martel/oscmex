// filepath: c:\codedev\auricleinc\oscmex\src\ServerImpl.h
#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <string>

#include "osc/Bundle.h"
#include "osc/Message.h"
#include "osc/Types.h"

namespace osc {

    class Message;
    class Bundle;

    using MethodId = int;
    using MethodHandler = std::function<void(const Message &)>;
    using BundleStartHandler = std::function<void(const Bundle &)>;
    using BundleEndHandler = std::function<void(const Bundle &)>;
    using ErrorHandler = std::function<void(int, const std::string &, const std::string &)>;

    // Used for storing information about a registered method
    struct Method {
        MethodId id;
        std::string pathPattern;
        std::string typeSpec;
        MethodHandler handler;
        std::regex pathRegex;
        bool isDefault;
    };

    class ServerImpl {
       public:
        ServerImpl(std::string port, Protocol protocol);
        ~ServerImpl();

        MethodId addMethod(const std::string &pathPattern, const std::string &typeSpec,
                           MethodHandler handler);
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
        bool matchMethod(const std::string &path, const std::string &types,
                         std::vector<Method *> &matchedMethods);
        // Additional helper functions
        static bool initializeNetworking();
        static void cleanupNetworking();
        static int getLastSocketError();
        std::regex convertPathToRegex(const std::string &pattern);
        static bool matchPattern(const std::string &pattern, const std::string &path);

        std::string port_;
        Protocol protocol_;
        SOCKET_TYPE socket_;
        std::map<MethodId, Method> methods_;
        MethodId nextMethodId_;
        std::mutex methodMutex_;
        BundleStartHandler bundleStartHandler_;
        BundleEndHandler bundleEndHandler_;
        ErrorHandler errorHandler_;
        size_t maxMessageSize_;             // Maximum size of OSC messages
        static bool networkingInitialized;  // Flag for networking initialization
    };

}  // namespace osc
