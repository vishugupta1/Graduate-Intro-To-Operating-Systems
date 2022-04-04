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

#define BUFSIZE 218

#define USAGE                                            \
  "usage:\n"                                             \
  "  transferserver [options]\n"                         \
  "options:\n"                                           \
  "  -f                  Filename (Default: 6200.txt)\n" \
  "  -h                  Show this help message\n"       \
  "  -p                  Port (Default: 10823)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"filename", required_argument, NULL, 'f'},
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}};



void send_contents(FILE* file, int sck) {
    char data[BUFSIZE] = { 0 };
    while (fgets(data, BUFSIZE, file) != NULL) {
        if (send(sck, data, sizeof(data), 0) == -1) {
            perror("Transfer error from server");
            exit(1);
        }
        bzero(data, BUFSIZE);
    }
    fclose(file);
}


int main(int argc, char **argv) {
  int option_char;
  int portno = 10823;          /* port to listen on */
  char *filename = "6200.txt"; /* file to transfer */

  setbuf(stdout, NULL);  // disable buffering

  // Parse and set command line arguments
  while ((option_char =
              getopt_long(argc, argv, "xp:hf:", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 'p':  // listen-port
        portno = atoi(optarg);
        break;
      default:
        fprintf(stderr, "%s", USAGE);
        exit(1);
      case 'h':  // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
      case 'f':  // file to transfer
        filename = optarg;
        break;
    }
  }

  if ((portno < 1025) || (portno > 65535)) {
    fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__,
            portno);
    exit(1);
  }

  if (NULL == filename) {
    fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
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
  if (listen(serverSocket, 1) < 0) {
      perror("Listening failed");
      exit(1);
  }



  // accept client connection and store client info into clientAddress
  if ((clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &addrlen)) < 0) {
      perror("Accepting failed");
      exit(1);
  }

 
  FILE* fp = fopen(filename, "r");
  send_contents(fp, clientSocket);

  // close client socket
  close(clientSocket);
 

  // close server socket
  close(serverSocket);

  return 0;
}
