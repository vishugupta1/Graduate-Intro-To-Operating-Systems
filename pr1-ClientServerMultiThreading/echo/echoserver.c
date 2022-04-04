#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFSIZE 218

#define USAGE                                                        \
  "usage:\n"                                                         \
  "  echoserver [options]\n"                                         \
  "options:\n"                                                       \
  "  -p                  Port (Default: 10823)\n"                    \
  "  -m                  Maximum pending connections (default: 5)\n" \
  "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"port", required_argument, NULL, 'p'},
    {"maxnpending", required_argument, NULL, 'm'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

int main(int argc, char** argv) {
    int option_char;
    int portno = 10823; /* port to listen on */
    int maxnpending = 5;

    // Parse and set command line arguments
    while ((option_char =
        getopt_long(argc, argv, "p:m:hx", gLongOptions, NULL)) != -1) {
        switch (option_char) {
        case 'p':  // listen-port
            portno = atoi(optarg);
            break;
        default:
            fprintf(stderr, "%s ", USAGE);
            exit(1);
        case 'm':  // server
            maxnpending = atoi(optarg);
            break;
        case 'h':  // help
            fprintf(stdout, "%s ", USAGE);
            exit(0);
            break;
        }
    }

    setbuf(stdout, NULL);  // disable buffering

    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__,
            portno);
        exit(1);
    }
    if (maxnpending < 1) {
        fprintf(stderr, "%s @ %d: invalid pending count (%d)\n", __FILE__, __LINE__,
            maxnpending);
        exit(1);
    }


    /* Socket Code Here */

    // create socket addresses
    struct addrinfo specs;
    struct addrinfo* serverinfo;
    struct sockaddr_storage clientAddress;
    int serverSocket;
    int clientSocket;
    int opt = 1;
    socklen_t addrlen = sizeof(clientAddress);
    char buffer[16];

    // declare specs 
    memset(&specs, 0, sizeof(specs));
    specs.ai_family = AF_UNSPEC;
    specs.ai_socktype = SOCK_STREAM;
    specs.ai_flags = AI_PASSIVE;


    // convert integer port to string 
    char port[20];
    sprintf(port, "%d", portno);

    // get linked list pointer serverinfo pointing to ip4 and ip6 addresses
    struct addrinfo* itr;

    // set port on serverinfo and configure port according to specs 
    if (getaddrinfo(NULL, port, &specs, &serverinfo) != 0) {
        fprintf(stderr, "\ngetaddrinfo error!\n");
    }


    // iterate through serverinfo structs (ipv4, ipv6)
    for (itr = serverinfo; itr != NULL; itr = itr->ai_next) {

        // try creating socket with current itr. if not then try next address space
        if ((serverSocket = socket(itr->ai_family, itr->ai_socktype, itr->ai_protocol)) < 0) {
            continue;
        }

        // clean up dangling socket
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            exit(1);
        }

        // bind new serverSocket to current serverinfo struct 
        // if failed close socket and try next serverinfo address space
        if (bind(serverSocket, itr->ai_addr, itr->ai_addrlen) < 0) {
            close(serverSocket);
            continue;
        }

        // if successful we can break from this loop 
        break;
    }

    if (itr == NULL) {
        perror("Socket failed");
        exit(1);
    }

    // we can free up this space 
    freeaddrinfo(serverinfo);

    // allow serversocket to listen up to maxnpending connection requests
    if (listen(serverSocket, maxnpending) < 0) {
        perror("Listening failed");
        exit(1);
    }

    // accept client connection and echo back message 
    while (maxnpending > 0) {
        // accept client connection and store client info into clientAddress
        if ((clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &addrlen)) < 0) {
            perror("Accepting failed");
            exit(1);
        }

        if (recv(clientSocket, buffer, 16, 0) < 0) {
            perror("Recieving failed");
            exit(1);
        }
        send(clientSocket, buffer, 16, 0);
        maxnpending--;

        // close client socket
        close(clientSocket);
    }

    // close server socket
    close(serverSocket);

    return 0;
}



  /*

  // create socket with serverinfo specs
  if ((serverSocket = socket(serverinfo->ai_family, serverinfo->ai_socktype, serverinfo->ai_protocol)) == 0) {
      perror("Socket failed");
      exit(1);
  }


  // force connect the port if still hanging from previous run 
  if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
      close(serverSocket);
      exit(1);
  }

  // bind port and ip address through serverinfo addr 
  if (bind(serverSocket, serverinfo->ai_addr, serverinfo->ai_addrlen) < 0) {
      perror("Bind failed");
      exit(1);
  }

  // allow serversocket to listen up to maxnpending connection requests
  if (listen(serverSocket, maxnpending) < 0) {
      perror("Listening failed");
      exit(1);
  }

  // accept client connection and echo back message 
  while (maxnpending > 0) {
      // accept client connection and store client info into clientAddress
      if ((clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &addrlen)) < 0) {
          perror("Accepting failed");
          exit(1);
      }

      if (recv(clientSocket, buffer, 16, 0) < 0) {
          perror("Recieving failed");
          exit(1);
      }
      send(clientSocket, buffer, 16, 0);
      maxnpending--;
      close(clientSocket);
  }

  freeaddrinfo(serverinfo)
  close(serverSocket);
  
  return 0;


  

  struct sockaddr_in6 address;
  struct sockaddr_in6 clientAddress; 
  int server;
  int new_socket;
  int addrlen = sizeof(clientAddress);
  int valread; 
  char buffer[16];
  int opt = 1;

  if ((server = socket(AF_INET6, SOCK_STREAM, 0)) == 0) {
      perror("Socket failed");
      exit(EXIT_FAILURE);
  }

  if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
      close(server);
      exit(1);
  }
  address.sin6_family = AF_INET6;
  address.sin6_addr = in6addr_any;
  //address.sin_addr.s_addr = inet_addr("127.0.0.1");
  address.sin6_port = htons(portno);

  if (bind(server, (struct sockaddr*)&address, sizeof(address)) < 0) {
      perror("bind failed");
      exit(EXIT_FAILURE);
  }

  if (listen(server, maxnpending) < 0) {
      perror("listen");
      exit(EXIT_FAILURE);
  }

  while (maxnpending > 0) {
      if ((new_socket = accept(server, (struct sockaddr*)&clientAddress, (socklen_t*)&addrlen)) < 0) {

          perror("accept");
          exit(EXIT_FAILURE);
      }

      valread = recv(new_socket, buffer, 16, 0);
      if (valread < 0)
          return -1;
      send(new_socket, buffer, 16, 0);
      maxnpending--;
  }

  */


