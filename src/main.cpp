#include "server.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  // flushing after every std::cout / std::cerr;
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // you can use print statements as follows for debugging,
  // they'll be visible while running tests;
  std::cout << "Logs from the program will appear here!" << std::endl;

  // parsing the --directory argument
  std::string directory = "";
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--directory" && i + 1 < argc) {
      directory = argv[i + 1];
      std::cout << "Files directory: " << directory << std::endl;
      break;
    }
  }

  int port = 4221;
  if (const char *env = std::getenv("PORT")) {
    int p = std::atoi(env);
    if (p > 0 && p < 65536)
      port = p;
  }

  // create and setup server
  Server server(directory, port);
  if (!server.setup()) {
    std::cerr << "Server setup falied" << std::endl;
    return 1;
  }

  // accept and handle one client
  server.start(); // runs forever, handles all the clients!!!l

  return 0;
}