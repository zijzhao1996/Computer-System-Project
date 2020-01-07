#pragma once
#include <exception>
#include <stdexcept>
#include <iostream>
#include <unordered_map>

#include <boost/network/protocol/http/server.hpp>

#include "document.h"

namespace http = boost::network::http;

class IndexException: public std::runtime_error {
public:
    IndexException(std::string msg) : std::runtime_error(msg.c_str()) {}
};

class Index;
typedef http::server<Index> http_server;

class Index {
public:
    /* Constructs a new Index. Pass memoryOnly to completely ignore disk (this
     * overrides loadFromDisk). You can pass loadFromDisk=false if you want to
     * delete the index from disk and build a new one. */
    Index(bool memoryOnly, bool loadFromDisk = true);

    /* Adds a document to the index. This function is not thread-safe. */
    void addDocument(Document document);

    /* Launches the archive web server (and doesn't return as long as the web
     * server is running). This function is NOT thread-safe, and accesses
     * `documents` without any locking. Ensure that `documents` isn't changed
     * by other threads while this function is executing. */
    void serve(unsigned short portNum);

    /* Called by the cpp-netlib server to handle an incoming request */
    void operator()(http_server::request const &request,
            http_server::connection_ptr connection);

    /* Print the contents of the index to stderr */
    void dumpToTerminal();

private:
    bool memoryOnly;
    std::unordered_map<std::string, Document> documents;

    void serveDocument(std::string url, http_server::connection_ptr connection,
            bool headersOnly);
    void dumpMetadata();
};
