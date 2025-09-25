#pragma once

#include <functional>
#include <string>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#else
typedef int socket_t;
#endif

class HTTPServer {
public:
    // Handler for GET requests
    using RequestHandler = std::function<std::string(const std::unordered_map<std::string, std::string>&)>;

    // Handler for POST requests that includes body
    using PostRequestHandler = std::function<std::string(
        const std::unordered_map<std::string, std::string>&,
        const std::string&)>;

    HTTPServer(int port);
    ~HTTPServer();

    // Register GET handler
    void register_handler(const std::string& path, RequestHandler handler);

    // Register POST handler with body support
    void register_post_handler(const std::string& path, PostRequestHandler handler);

    void start();
    void stop();

private:
    int port_;
    bool running_;
    std::thread server_thread;
    std::unordered_map<std::string, RequestHandler> get_handlers_;
    std::unordered_map<std::string, PostRequestHandler> post_handlers_;

    void handle_connection(socket_t client_socket);
    void parse_request(const std::string& request,
                       std::string& method,
                       std::string& path,
                       std::unordered_map<std::string, std::string>& headers,
                       std::unordered_map<std::string, std::string>& params,
                       std::string& body);
};