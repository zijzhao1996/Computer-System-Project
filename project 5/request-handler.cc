/**
 * File: request-handler.cc
 * ------------------------
 * Provides the implementation for the HTTPRequestHandler class.
 */

#include "request-handler.h"
#include "client-socket.h"
#include <unistd.h>
#include <netdb.h>
#include <mutex>
# include <string>
using namespace std;

HTTPRequestHandler::HTTPRequestHandler() throw (HTTPProxyException) : usingProxy(false) {
  blacklist.addToBlacklist("blocked-domains.txt");
}

void HTTPRequestHandler::serviceRequest(const pair<int, string>& connection) throw() {
  const int& clientfd = connection.first;
  const string& clientIPAddress = connection.second;
  sockbuf clientsb(clientfd);
  iosockstream clientStream(&clientsb);
  HTTPRequest request(usingProxy);
  HTTPResponse response;
  try {
    ingestRequest(clientStream, clientIPAddress, request);
  } catch (const HTTPBadRequestException& e) {
    sendResponse(clientStream, createErrorResponse(400, e.what()));
    return;
  } catch (const HTTPCircularProxyChainException& e) {
    sendResponse(clientStream, createErrorResponse(504, e.what()));
    return;
  }
  if (!blacklist.serverIsAllowed(request.getServer())) {
    sendResponse(clientStream, createErrorResponse(403, "Forbidden Content"));
    return;
  }
  mutex& requestLock = cache.getLock(request);
  lock_guard<mutex> lg(requestLock);
  if (cache.containsCacheEntry(request, response)) {
    sendResponse(clientStream, response);
    return;
  }
  const string& server = usingProxy ? proxyServer : request.getServer();
  unsigned short port = usingProxy ? proxyPortNumber : request.getPort();
  int serverfd = createClientSocket(server, port);
  if (serverfd == kClientSocketError) {
    sendResponse(clientStream, createErrorResponse(404, "Server Not Found"));
    return;
  }
  sockbuf serversb(serverfd);
  iosockstream serverStream(&serversb);
  sendRequest(serverStream, request);
  ingestResponse(serverStream, request, response);
  sendResponse(clientStream, response);
  if (cache.shouldCache(request, response)) {
    cache.cacheEntry(request, response);
  }
}

void HTTPRequestHandler::clearCache() {
  cache.clear();
}

void HTTPRequestHandler::setCacheMaxAge(long maxAge) {
  cache.setMaxAge(maxAge);
}

void HTTPRequestHandler::setProxy(const std::string& server, unsigned short port) {
  proxyServer = server;
  proxyPortNumber = port;
  usingProxy = true;
}

void HTTPRequestHandler::ingestRequest(istream& instream,
  const string& clientIPAddress, HTTPRequest& request) {
  request.ingestRequestLine(instream);
  request.ingestHeader(instream, clientIPAddress);
  request.ingestPayload(instream);
  request.addHeader("x-forwarded-proto", "http");
  const string& ipList = request.getHeaderValueAsString("x-forwarded-for");
  if (findCircularProxyChain(ipList, clientIPAddress)) {
    throw HTTPCircularProxyChainException();
  }

  if (ipList.empty()){
    request.addHeader("x-forwarded-for", clientIPAddress);
  }
  else {
    request.addHeader("x-forwarded-for", ipList + ", " + clientIPAddress);
  }
}

bool HTTPRequestHandler::findCircularProxyChain(const string& ipList, const string& ip) {
  istringstream ss(ipList);
  string element;
  while (getline(ss, element, ',')) {
    if (element == ip) return true;
  }
  return false;
}

void HTTPRequestHandler::ingestResponse(istream& instream,
  const HTTPRequest& request, HTTPResponse& response) {
  response.ingestResponseHeader(instream);
  if (request.getMethod() == "HEAD") return;
  response.ingestPayload(instream);
}

HTTPResponse HTTPRequestHandler::createErrorResponse(int code, const string& message) {
  HTTPResponse response;
  response.setResponseCode(code);
  response.setProtocol("HTTP/1.0");
  response.setPayload(message);
  return response;
}

void HTTPRequestHandler::sendRequest(ostream& outstream, const HTTPRequest& request) {
  outstream << request;
  outstream.flush();
}

void HTTPRequestHandler::sendResponse(ostream& outstream, const HTTPResponse& response) {
  outstream << response;
  outstream.flush();
}