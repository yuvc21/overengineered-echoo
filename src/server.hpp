#ifndef SERVER_HPP
#define SERVER_HPP

// Internet networking structures (sockaddr_in, AF_INET, INADDR_ANY)
#include <netinet/in.h>
#include <string>

// For creating concurrent threads to handle multiple clients
#include <thread>

// For dynamic arrays (not used yet, but good for future)
#include <vector>

// Server class - manages HTTP server on port 4221
class Server {
private:
  // File descriptor for the server socket
  // -1 means not initialized yet
  int server_fd;

  // Directory path where files are stored (for /files/ endpoint)
  // Example: "/tmp" means files are in /tmp/filename
  std::string filesDirectory;

  int port;

public:
  // Constructor with optional directory parameter
  // Default is empty string "" if no directory provided
  // Usage: Server myServer("/tmp");
  Server(const std::string &directory = "", int port = 4221);

  // Destructor - automatically called when Server object is destroyed
  // Closes the server socket to free resources
  ~Server();

  // Creates socket, sets options, binds to port 4221, starts listening
  // Returns: true if successful, false if any step fails
  // Call this BEFORE start()
  bool setup();

  // Waits for and accepts a new client connection
  // Returns: client file descriptor (>= 0) or -1 on error
  // This function BLOCKS until a client connects
  int acceptClient();

  // Handles a single client request in a separate thread
  // Reads HTTP request, generates response, sends it, closes connection
  // Parameter: client_fd - file descriptor for connected client socket
  void handleClient(int client_fd);

  // Main server loop - runs forever until manually stopped (Ctrl+C)
  // Accepts clients in infinite loop
  // Spawns new thread for each client (concurrent handling)
  // Never returns - runs until program terminates
  void start();

  // Getter method to retrieve the directory path
  // Returns: reference to filesDirectory string
  // const means: won't modify the Server object
  const std::string &getDirectory() const { return filesDirectory; }
};

#endif // SERVER_HPP - End of header guard