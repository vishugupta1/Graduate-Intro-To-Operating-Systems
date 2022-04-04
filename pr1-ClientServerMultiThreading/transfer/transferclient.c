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

#define USAGE                                                \
  "usage:\n"                                                 \
  "  transferclient [options]\n"                             \
  "options:\n"                                               \
  "  -s                  Server (Default: localhost)\n"      \
  "  -p                  Port (Default: 10823)\n"            \
  "  -o                  Output file (Default cs6200.txt)\n" \
  "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {{"server", required_argument, NULL, 's'},
                                       {"port", required_argument, NULL, 'p'},
                                       {"output", required_argument, NULL, 'o'},
                                       {"help", no_argument, NULL, 'h'},
                                       {NULL, 0, NULL, 0}};

/* Main ========================================================= */

void save_file(int sck, char* filename) {
    int n; 
    FILE* fp;
   //char* filename = "file2.txt";
    char buffer[BUFSIZE];

    fp = fopen(filename, "w");

    while (1) {
        n = recv(sck, buffer, BUFSIZE, 0);
        if (n <= 0) {
            break;
            return;
        }
        fprintf(fp, "%s", buffer);
       // bzero(buffer, BUFSIZE);
    }
    fclose(fp);
}


int main(int argc, char **argv) {
  int option_char = 0;
  char *hostname = "localhost";
  unsigned short portno = 10823;
  char *filename = "cs6200.txt";

  setbuf(stdout, NULL);

  // Parse and set command line arguments
  while ((option_char =
              getopt_long(argc, argv, "s:xp:o:h", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 's':  // server
        hostname = optarg;
        break;
      case 'p':  // listen-port
        portno = atoi(optarg);
        break;
      default:
        fprintf(stderr, "%s", USAGE);
        exit(1);
      case 'o':  // filename
        filename = optarg;
        break;
      case 'h':  // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
    }
  }

  if (NULL == hostname) {
    fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
    exit(1);
  }

  if (NULL == filename) {
    fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
    exit(1);
  }

  if ((portno < 1025) || (portno > 65535)) {
    fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__,
            portno);
    exit(1);
  }

  /* Socket Code Here */

   // declare variables 
  int clientsocket;
  struct addrinfo specs, * clientinfo;
  char buffer[16];

  // set specs 
  memset(&specs, 0, sizeof(specs));
  specs.ai_family = AF_UNSPEC;
  specs.ai_socktype = SOCK_STREAM;
  specs.ai_flags = AI_PASSIVE;

  // convert integer port to string 
  char port[20];
  sprintf(port, "%d", portno);

  // get linked list pointer serverinfo pointing to ip4 and ip6 addresses
  // set port on clientinfo 
  if (getaddrinfo(NULL, port, &specs, &clientinfo) != 0) {
      fprintf(stderr, "\ngetaddrinfo error!\n");
  }

  // declare itr
  struct addrinfo* itr;

  // get either ipv4, or ipv6 and try to connect
  for (itr = clientinfo; itr != NULL; itr = itr->ai_next) {
      // create socket with current pointer
      if ((clientsocket = socket(itr->ai_family, itr->ai_socktype, itr->ai_protocol)) < 0) {
          //fprintf(stderr, "\nFailed to create socket!\n");
          continue;
      }

      // connect newly created socket with client address 
      if (connect(clientsocket, itr->ai_addr, itr->ai_addrlen) < 0) {
          close(clientsocket);
          //perror("client failed to connect");
          continue;
      }
      break;
  }

  // check if itr points to null
  if (itr == NULL) {
      perror("itr is null!");
      exit(1);
  }

  save_file(clientsocket, filename);

  // we can free up this space 
  freeaddrinfo(clientinfo);
  close(clientsocket);
}
