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

/* A buffer large enough to contain the longest allowed string */
#define BUFSIZE 218

#define USAGE                                                          \
  "usage:\n"                                                           \
  "  echoclient [options]\n"                                           \
  "options:\n"                                                         \
  "  -s                  Server (Default: localhost)\n"                \
  "  -p                  Port (Default: 10823)\n"                      \
  "  -m                  Message to send to server (Default: \"Hello " \
  "Summer.\")\n"                                                       \
  "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"server", required_argument, NULL, 's'},
    {"port", required_argument, NULL, 'p'},
    {"message", required_argument, NULL, 'm'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0} };

/* Main ========================================================= */
int main(int argc, char** argv) {
    int option_char = 0;
    char* hostname = "localhost";
    unsigned short portno = 10823;
    char* message = "Hello Summer!!";

    // Parse and set command line arguments
    while ((option_char =
        getopt_long(argc, argv, "s:p:m:hx", gLongOptions, NULL)) != -1) {
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
        case 'm':  // message
            message = optarg;
            break;
        case 'h':  // help
            fprintf(stdout, "%s", USAGE);
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

    if (NULL == message) {
        fprintf(stderr, "%s @ %d: invalid message\n", __FILE__, __LINE__);
        exit(1);
    }

    if (NULL == hostname) {
        fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
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

    // try sending message through socket
    if (send(clientsocket, message, strlen(message), 0) < 0) {
        perror("\nUnable to send message\n");
        exit(1);
    };

    // try recieving message through socket and store in buffer 
    if (recv(clientsocket, buffer, 16, 0) < 0) {
        perror("\nError while recieving message\n");
        exit(1);
    }

    printf("%s", buffer);

    // we can free up this space 
    freeaddrinfo(clientinfo);
    close(clientsocket);
}





    //int sock = 0;
    //struct sockaddr_in serv_addr;
    //char buffer[16];


    //if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    //    printf("\n Socket creation error \n");
    //    return -1;
    //}

    //serv_addr.sin_family = AF_INET;
    //serv_addr.sin_port = htons(portno);
    //serv_addr.sin_addr.s_addr = INADDR_ANY;

    //// serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    ////hostname = "192.168.1.143";


    ///*int val;
    //if ((val = inet_pton(AF_INET, hostname, &serv_addr.sin_addr)) <= 0) {
    //    printf("%d", val);
    //    printf("\nInvalid address / Address not supported\n");
    //    return -1;
    //} */

    //if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) {
    //    printf("\nConnection Failed\n");
    //    return -1;
    //}

    //if (send(sock, message, strlen(message), 0) < 0) {
    //    printf("\nUnable to send message\n");
    //    return -1;
    //};

    //if (recv(sock, buffer, 16, 0) < 0) {
    //    printf("\nError while recieving message\n");
    //    return -1;
    //}

    //printf("%s", buffer);

    //close(sock);



