#include "server.hpp"
#include "http_parser.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <cstring>
#include <iostream>
#include <sys/time.h>
#include <sys/socket.h> // send(), recv(), socket(), bind(), listen(), accept()
#include <thread>
// #include <sys/socket.h> // send(), recv(), socket(), bind(), listen(),
// accept()
//   these calls are system calls they are part of operating system's(linux
//   here)
//  API for network programming not function of CPP and are somewhat different
//  for windows
#include <unistd.h> // close(), read(), write()

/* When you call:
send(client_socket, response.c_str(), response.length(), 0);

 Your program asks the Linux kernel:
 "Please send this data through network socket #X"
*/

constexpr size_t kMaxRequestBytes = 1024 * 1024;

// this takes in the string
// this provides number to the readRequest so it can stop when required
long contentLength(const std::string &headers) {
  std::string lower = headers;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return tolower(c); });
  size_t pos = lower.find("\r\ncontent-length:");
  if (pos == std::string::npos)
    return 0;
  pos += 17;
  size_t end = lower.find("\r\n", pos);
  if (end == std::string::npos)
    return -1;
  std::string lengthText = headers.substr(pos, end - pos);
  size_t first = lengthText.find_first_not_of(" \t"),
         last = lengthText.find_last_not_of(" \t");
  if (first == std::string::npos)
    return -1;
  lengthText = lengthText.substr(first, last - first + 1);
  if (lengthText.size() > 15 ||
      lengthText.find_first_not_of("0123456789") != std::string::npos)
    return -1;
  return std::stol(lengthText);
}

// reads one full request
// returns 0(fine), 400(time-out/malformed/closed-), 413(too-large)
int readRequest(int fd, std::string &request) {
  char buf[4096];
  size_t header_end = std::string::npos;
  long need = -1;
  while (true) {
    if (header_end != std::string::npos && (long)request.size() >= need) {
      return 0;
    }
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0)
      return 400; // closed, error, or SO_RCVTIMEO firex
    request.append(buf, n);
    if (request.size() > kMaxRequestBytes)
      return 413;
    if (header_end == std::string::npos) {
      header_end = request.find("\r\n\r\n");
      if (header_end != std::string::npos) {
        long len = contentLength(request.substr(0, header_end + 2));
        if (len < 0)
          return 400;
        if ((size_t)len + header_end + 4 > kMaxRequestBytes)
          return 413;
        need = (long)header_end + 4 + len;
      }
    }
  }
}

Server::Server(const std::string &directory)
    : server_fd(-1), filesDirectory(directory) {}

Server::~Server() {
  if (server_fd >= 0) {
    close(server_fd);
  }
}

bool Server::setup() {
  // create socket;
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "Failed to create server socket\n";
    return false;
  }

  // since the tester restarts your program quite often, setting SO_REUSEADDR;
  //  ensures that we don't run into 'Address already in use' error;
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    std::cerr << "setsocket failed\n";
    return false;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) !=
      0) {
    std::cerr << "Failed to bind port 4221\n";
    return false;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return false;
  }
  return true;
}

int Server::acceptClient() {
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);

  // it blocks code up till here until a client is connected;
  std::cout << "Waiting for a client to connect..." << std::endl;
  int client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                         (socklen_t *)&client_addr_len);
  std::cout << "Client Connected" << std::endl;

  return client_fd;
}

void Server::start() {
  std::cout << "Server is running on port 4221...." << std::endl;

  // infinite loop to keep accepting connections
  while (true) {
    int client_fd = acceptClient();

    if (client_fd < 0) {
      std::cerr << "Failed to accept client" << std::endl;
      continue; // skipping the current iteration, trying again
    }

    // starting new thread to handle this client concurrently!!!
    std::thread clientThread([this, client_fd] { handleClient(client_fd); });

    // detatching the thread(runs independently, doesn't block)
    clientThread.detach();

    // server immidiately ready for next client
    std::cout << "Client spawned in new thread, ready for next..." << std::endl;
  }
}
void Server::handleClient(int client_fd) {
  // reading the request
  timeval tv{5, 0};
  setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  std::string request;
  int rc = readRequest(client_fd, request);
  if (rc != 0) {
    const char *resp;
    if (rc == 413) {
      resp = "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\n\r\n";
    } else {
      resp = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
    }
    send(client_fd, resp, strlen(resp), MSG_NOSIGNAL);
    close(client_fd);
    return;
  }

  // extracting path
  std::string path = HttpParser::extractPath(request);

  // extracting the method
  std::string method = HttpParser::extractMethod(request);

  // extracting body
  std::string body = HttpParser::extractBody(request);

  // extracting acceptEncoding
  std::string acceptEncoding = HttpParser::extractAcceptEncoding(request);

  std::cout << "Request: " << method << " " << path << std::endl;
  std::cout << "Accept-Encoding: " << acceptEncoding << std::endl;

  // generating responses with method support
  std::string response = HttpParser::generateResponse(
      method, path, body, filesDirectory, acceptEncoding);

  // sending response
  send(client_fd, response.c_str(), response.length(), 0);

  // closing the client connection
  close(client_fd);
}