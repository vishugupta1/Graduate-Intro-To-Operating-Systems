
#include <stdlib.h>
#include "gfclient-student.h"

/*
 * Modify this file to implement the interface specified in
 * gfclient.h.
 */

//#define BUFSIZE 100000


typedef void (*headerfunc)(void*, size_t, void*);
typedef void (*writefunc)(void*, size_t, void*);
int gfc_getMessages(gfcrequest_t** gfr);

// optional function for cleaup processing.
void gfc_cleanup(gfcrequest_t** gfr) {
    //free((*gfr)->filePath);
   // free((*gfr)->headerarg);
    //free((*gfr)->writearg);
    //close((*gfr)->clientsocket);
    //free(((*gfr)->filePath));
    //(*gfr)->filePath = NULL;
    //free((*gfr)->writearg);
    //(*gfr)->writearg = NULL;
    //free((*gfr)->wfunction);
    //(*gfr)->wfunction = NULL;
    //close((*gfr)->clientsocket);
    free((*gfr));
    (*gfr) = NULL;
}

struct gfcrequest_t {
    gfstatus_t status;
    void* headerarg;
    void* writearg;
    // void* headerData;
    // size_t headerSize;
    // void* fileContent;
    // size_t fileContentSize;
    headerfunc hfunction;
    writefunc wfunction;
    const char* filePath;
    size_t fileLen;
    size_t bytesRecieved;
    unsigned short port;
    const char* server;
    int clientsocket;
};


gfcrequest_t* gfc_create() {
    struct gfcrequest_t* request = (struct gfcrequest_t*)malloc(sizeof(struct gfcrequest_t));
    return (gfcrequest_t*)request;
}

size_t gfc_get_bytesreceived(gfcrequest_t** gfr) {
    return (*gfr)->bytesRecieved;
}

size_t gfc_get_filelen(gfcrequest_t** gfr) {
    return (*gfr)->fileLen;
}

gfstatus_t gfc_get_status(gfcrequest_t** gfr) {
    return (*gfr)->status;
}

void gfc_global_init() {}

void gfc_global_cleanup() {}

int gfc_createsocket(gfcrequest_t** gfr) {
    // -------------------------------------------------------------------------------------------
    // create socket from client to server
    //int clientsocket;
    struct addrinfo specs, * clientinfo;
    int opt = 1;
    struct addrinfo* itr;
    memset(&specs, 0, sizeof(specs));
    specs.ai_family = AF_UNSPEC;
    specs.ai_socktype = SOCK_STREAM;
    specs.ai_flags = AI_PASSIVE;
    char port[20];
    sprintf(port, "%d", (*gfr)->port);
    if (getaddrinfo("localhost", port, &specs, &clientinfo) != 0) {
        fprintf(stderr, "\ngetaddrinfo error!\n");
    }
    for (itr = clientinfo; itr != NULL; itr = itr->ai_next) {
        if (((*gfr)->clientsocket = socket(itr->ai_family, itr->ai_socktype, itr->ai_protocol)) < 0) {
            continue;
        }
        // clean up dangling socket
        if (setsockopt((*gfr)->clientsocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            continue;
        }
        if (connect((*gfr)->clientsocket, itr->ai_addr, itr->ai_addrlen) < 0) {
            close((*gfr)->clientsocket);
            continue;
        }
        break;
    }
    if (itr == NULL) {
        perror("itr is null!");
        exit(1);
    }
    freeaddrinfo(clientinfo);
    //free(itr);
}

int gfc_perform(gfcrequest_t** gfr) {

    // -------------------------------------------------------------------------------------------
    // create socket
    gfc_createsocket(gfr);

    // -------------------------------------------------------------------------------------------
    // send header request to server 
    char messageHeader[1000];
    sscanf(messageHeader, "GETFILE GET %s\r\n\r\n", (*gfr)->filePath);
    //snprintf(messageHeader, length, "GETFILE GET %s\r\n\r\n", (*gfr)->filePath);
    printf("%s", messageHeader);
    /*
    size_t bytes_written = 0;
    while (bytes_written <= length - 1) {
        // hello\0
        // length = 6
        // bytes_written = 0
        char subString[length];
        memcpy(subString, &messageHeader[bytes_written], length - bytes_written);
        bytes_written += send((*gfr)->clientsocket, messageHeader, length, 0);
    }
    */

    if ((send((*gfr)->clientsocket, messageHeader, length - 1, 0) < 0)) {
        (*gfr)->status = GF_ERROR;
        return 1;
    }



    /*
    int sum = 0;
    char buffer[256];
    int n;
    memset(buffer, '\0', 256);
    strncpy(buffer, &messageHeader[0], 255);
    do{
        n = send((*gfr)->clientsocket, strtok(buffer, "\0"), 255, 0);
        sum += n;
        memset(buffer, '\0', 256);
        strncpy(buffer, &messageHeader[n], 255);
    } while (sum < length && n > 0);
    printf("********%d %d******", length, sum);
    if (n < 0 && sum < length - 1) {
        (*gfr)->status = GF_ERROR;
    }*/



    // -------------------------------------------------------------------------------------------
    // recieve messages from server 
    int feedback;
    feedback = gfc_getMessages(gfr);

    // -------------------------------------------------------------------------------------------
    // close socket 
    //close((*gfr)->clientsocket);
    //free(messageHeader);
    //messageHeader = NULL;
    return feedback;
}

void gfc_set_headerarg(gfcrequest_t** gfr, void* headerarg) {
    (*gfr)->headerarg = headerarg;
}

void gfc_set_headerfunc(gfcrequest_t** gfr, void (*headerfunc)(void*, size_t, void*)) {
    (*gfr)->hfunction = headerfunc;
}

void gfc_set_path(gfcrequest_t** gfr, const char* path) {
    (*gfr)->filePath = path;
}

void gfc_set_port(gfcrequest_t** gfr, unsigned short port) {
    (*gfr)->port = port;
}

void gfc_set_server(gfcrequest_t** gfr, const char* server) {
    (*gfr)->server = server;
}

void gfc_set_writearg(gfcrequest_t** gfr, void* writearg) {
    (*gfr)->writearg = writearg;
}

void gfc_set_writefunc(gfcrequest_t** gfr, void (*writefunc)(void*, size_t, void*)) {
    (*gfr)->wfunction = writefunc;
}

const char* gfc_strstatus(gfstatus_t status) {
    const char* strstatus = NULL;
    switch (status) {
    default: {
        strstatus = "UNKNOWN";
    } break;

    case GF_INVALID: {
        strstatus = "INVALID";
    } break;

    case GF_FILE_NOT_FOUND: {
        strstatus = "FILE_NOT_FOUND";
    } break;

    case GF_ERROR: {
        strstatus = "ERROR";
    } break;

    case GF_OK: {
        strstatus = "OK";
    } break;
    }
    return strstatus;
}

int gfc_getMessages(gfcrequest_t** gfr) {

    size_t numRecv = 0;
    char recvBuffer[256]; 
    char header[1000];

    // recieve header
    do {
        memset(&recvBuffer, 0, 256);
        if (numRecv = recv((*gfr)->clientsocket, &recvBuffer, 256, 0) <= 0) {
            (*gfr)->status = GF_INVALID;
            return -1;
        }
        printf("buffer %s %lu", recvBuffer, numRecv);
        strcat(header, recvBuffer);
        printf(" **** header %s ****", header);
        //(*gfr)->bytesRecieved += numRecv;
    } while (strstr(header, "\r\n\r\n") == NULL);


    char* scheme = strtok(header, " ");
    char* status = strtok(NULL, " ");
    if (strcmp(scheme, "GETFILE") != 0) {
        (*gfr)->status = GF_INVALID;
        return -1;
    }
    else if (strcmp(strtok(status, "\r\n\r\n"), "FILE_NOT_FOUND") == 0) {
        (*gfr)->status = GF_FILE_NOT_FOUND;
        return 0;
    }
    else if (strcmp(strtok(status, "\r\n\r\n"), "ERROR") == 0) {
        printf("****************** IM RETURNING ERRROR");
        (*gfr)->status = GF_ERROR;
        return 0;
    }
    else if (strcmp(status, "INVALID") == 0) {
        (*gfr)->status = GF_INVALID;
        return 0;
    }
    else {
        (*gfr)->status = GF_OK;
        strtok(status, " ");
        strtok(NULL, " ");
        (*gfr)->fileLen = (size_t)(atoi(strtok(NULL, "\r\n\r\n")));
        printf("%zu", (*gfr)->fileLen);
        char* restMessage = strtok(NULL, "\r\n\r\n");
        (*gfr)->bytesRecieved = strlen(restMessage);
        if ((*gfr)->bytesRecieved > 0) {
            (*gfr)->wfunction(restMessage, strlen(restMessage), (*gfr)->writearg);
        }

        while ((*gfr)->bytesRecieved < (*gfr)->fileLen) {
            size_t recvBytes;
            char buffer[65000];
            recvBytes = recv((*gfr)->clientsocket, buffer, 65000, 0);
            (*gfr)->bytesRecieved += recvBytes;
            if (recvBytes <= 0 && (*gfr)->bytesRecieved < (*gfr)->fileLen) {
                printf("****************** IM RETURNING premature closed");
                (*gfr)->status = GF_ERROR;
                return 0;
            }
            else {
                (*gfr)->wfunction(buffer, recvBytes, (*gfr)->writearg);
            }
        }
    }

    //// recieve body 
    //int sum = 0;
    //do {
    //    memset(recvBuffer, '\0', 1000);
    //    size_t numRecv = recv((*gfr)->clientsocket, recvBuffer, 999, 0);
    //    sum += numRecv;
    //    if (numRecv < 0 && sum < (*gfr)->fileLen) {
    //        (*gfr)->status = GF_ERROR;
    //        return -1;
    //    }
    //    else {
    //        (*gfr)->wfunction(recvBuffer, strlen(recvBuffer), (*gfr)->writearg);
    //    }
    //} while (sum < (*gfr)->fileLen); 

    //(*gfr)->bytesRecieved = sum; 
    //return 1;
}


//cleanup(gfr);


   /*while (numRecv = recv((*gfr)->clientsocket, recvBuffer, sizeof(recvBuffer), 0) > 0) {

       (*gfr)->bytesRecieved += numRecv;
       // header flag is on, recieving first message
       if (header_flag) {
           char* header = (char*)malloc(numRecv + 1);
           strcpy(header, recvBuffer);
           char* scheme = strtok(header, " ");
           char* status = strtok(NULL, " ");

           if (strcmp(scheme, "GETFILE") != 0) {
               (*gfr)->status = GF_INVALID;
               exit(1);
           }

           if (strcmp(status, "FILE_NOT_FOUND") == 0) {
               (*gfr)->status = GF_FILE_NOT_FOUND;
               exit(1);
           }
           if (strcmp(status, "ERROR") == 0) {
               (*gfr)->status = GF_ERROR;
               exit(1);
           }

           if (strcmp(status, "OK") == 0) {
               char* fileLen = strtok(strtok(NULL, " "), "\r\n\r\n");
               (*gfr)->fileLen = atoi(fileLen);
               header_flag = 0;
               char* content = strtok(NULL, "\r\n\r\n");
               char* header = (char*)malloc(strlen(scheme) + 1 + strlen(status) + 1 + strlen(fileLen) + 5);
               strcpy(header, "GETFILE OK ");
               strcat(header, fileLen);
               strcat(header, "/r/n/r/n");
               //(*gfr)->headerData = header;
               //(*gfr)->headerSize = strlen(header);
               printf("%s", header);
               printf("%s", content);
               (*gfr)->hfunction(header, strlen(header), (*gfr)->headerarg);
               (*gfr)->wfunction(content, strlen(content), (*gfr)->writearg);
           }
       }
       else { // recieve rest of messages
           (*gfr)->wfunction(recvBuffer, numRecv, (*gfr)->writearg);
       }*/