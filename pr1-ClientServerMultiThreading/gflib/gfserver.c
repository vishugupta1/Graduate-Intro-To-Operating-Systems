
#include "gfserver-student.h"
#include <stdlib.h>

/*
 * Modify this file to implement the interface specified in
 * gfserver.h.
 */


#define BUFSIZE 100000

//typedef gfh_error_t (*handler)(gfcontext_t**, const char*, void*);

struct gfcontext_t {
    //gfstatus_t status;
    int clientSocket;
    //char scheme[100];
    //char method[100];
    //char* path;
    //size_t fileLength;
};

struct gfserver_t {
    //struct gfcontext_t ;
    //handler handle;
    gfh_error_t(*handler)(gfcontext_t**, const char*, void*);
    void* arg;
    unsigned short serverPort;
    //int serverSocket;
    int maxPending;
};

void gfs_abort(gfcontext_t **ctx){
    close((*ctx)->clientSocket);
    free(*ctx);
    *ctx = NULL;
}

gfserver_t* gfserver_create(){
    struct gfserver_t* request = (struct gfserver_t*)malloc(sizeof(struct gfserver_t)); 
    return (gfserver_t*)request;
}

ssize_t gfs_send(gfcontext_t** ctx, const void* data, size_t len) {
    ssize_t n;
    if (n = send((*ctx)->clientSocket, data, len, 0) == -1) {
        perror("Transfer error from server");
        return -1;
    }
    return n;
}

ssize_t gfs_sendheader(gfcontext_t** ctx, gfstatus_t status, size_t file_len) {
    char headerMessage[1000];
    ssize_t n;
    //memset(headerMessage, '\0', 1000);
    if (status == GF_OK) {
        sprintf(headerMessage, "GETFILE %s %zu\r\n\r\n", "OK", file_len);
    }
    else if (status == GF_ERROR) {
        sprintf(headerMessage, "GETFILE %s\r\n\r\n", "ERROR");
    }
    else if (status == GF_FILE_NOT_FOUND) {
        sprintf(headerMessage, "GETFILE %s\r\n\r\n", "FILE_NOT_FOUND");
    }
    else if (status == GF_INVALID) {
        sprintf(headerMessage, "GETFILE %s\r\n\r\n", "INVALID");
    }
    if (n = send((*ctx)->clientSocket, headerMessage, strlen(headerMessage), 0) == -1) {
        perror("Transfer error from server");
        return -1;
    }
    //printf("******************************** %s", headerMessage);
    return n;
}


void gfserver_serve(gfserver_t** gfs) {

    //gfcontext_t* ctxx = (gfcontext_t*)malloc(sizeof(gfcontext_t));

    // ---------------------------------------------------------------------------------------------------
    // create server socket and allow listening up to maxPending number of clients 
    char recvBuffer[MAX_REQUEST_LEN];
    //char filepath[MAX_REQUEST_LEN];
    char header[MAX_REQUEST_LEN];
    struct addrinfo specs;
    struct addrinfo* serverinfo;
    struct sockaddr_storage clientAddress;
    int serverSocket;
    int clientSocket;
    int opt = 1;
    socklen_t addrlen = sizeof(clientAddress);
    //char buffer[16];
    memset(&specs, 0, sizeof(specs));
    specs.ai_family = AF_UNSPEC;
    specs.ai_socktype = SOCK_STREAM;
    specs.ai_flags = AI_PASSIVE;
    char port[20];
    sprintf(port, "%d", (*gfs)->serverPort);
    struct addrinfo* itr;
    if (getaddrinfo("localhost", port, &specs, &serverinfo) != 0) {
        fprintf(stderr, "\ngetaddrinfo error!\n");
    }
    for (itr = serverinfo; itr != NULL; itr = itr->ai_next) {
        if ((serverSocket = socket(itr->ai_family, itr->ai_socktype, itr->ai_protocol)) < 0) {
            perror("error at socket creation");
            continue;
        }
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            //perror("error at setsockopt");
            //continue;
        }
        if (bind(serverSocket, itr->ai_addr, itr->ai_addrlen) < 0) {
            //close(serverSocket);
            //perror("Error at bind");
            continue;
        }
        break;
    }
    freeaddrinfo(serverinfo);
    if (itr == NULL) {
        //perror("Socket failed");
        //exit(1);
    }
    if (listen(serverSocket, (*gfs)->maxPending) < 0) {
        perror("Listening failed");
        //exit(1);
    }


    // ---------------------------------------------------------------------------------------------------
    // accept client connections and write data to clients
    while (1) {
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &addrlen);

        gfcontext_t* ctx = (gfcontext_t*)malloc(sizeof(gfcontext_t));
        ctx->clientSocket = clientSocket; 

        // ---------------------------------------------------------------------------------------------------
        // handle incoming header from client
        int numRecv;
 
        printf("*******");
        memset(&header, 0, sizeof header);







        int sum = 0;
        do {
            memset(&recvBuffer, 0, sizeof recvBuffer);
            //numRecv = 0;
            if ((numRecv = recv(ctx->clientSocket, recvBuffer, MAX_REQUEST_LEN, 0)) < 0) {
                perror("error");
            }
            recvBuffer[numRecv] = '\0';
            sum += numRecv; 
            
            strcat(header, recvBuffer);
        } while (strstr(header, "\r\n\r\n") == NULL && (sum < MAX_REQUEST_LEN));
        // ---------------------------------------------------------------------------------------------------

        // ---------------------------------------------------------------------------------------------------
        // parse header
        printf("++++++++++++++++++++");
        printf("********%s", header);
        char filepath[MAX_REQUEST_LEN];
        if (sscanf(header, "GETFILE GET /%s \r\n\r\n", filepath) == 1) {
            (*gfs)->handler(&ctx, filepath, (*gfs)->arg);
        }
        else {
            gfs_sendheader(&ctx, GF_INVALID, 0);
        }

    }




    //    char* scheme = strtok(header, "/");
    //    if (strcmp(scheme, "GETFILE GET") != 0) {
    //        (*gfr)->status = GF_INVALID;
    //        return;
    //    }

    //    char* method = strtok(strtok(NULL, "/"), "/r/n/r/n"));





    //    size_t numRecv = recv(clientSocket, recvBuffer, 999, 0);
    //    recvBuffer[numRecv] = '\0'; 
    //    char* filepath;
    //    filepath = strtok(recvBuffer, "/");
    //    filepath = strtok(NULL, "\"");
    //  /*  
    //    char* scheme = strtok(recvBuffer, " ");
    //    char* method = strtok(NULL, " ");
    //    char* filepath = strtok(strtok(NULL, " "), "\r\n\r\n");*/


    //    (*gfs)->handle(ctx, filepath, (*gfs)->arg);



    //    //do {
    //    //    memset(recvBuffer, '\0', 10000);
    //    //    size_t numRecv = recv(clientSocket, recvBuffer, 9999, 0); 
    //    //    //(*gfs)->handle(ctx, (*ctx)->path, (*gfs)->arg)
    //    //    printf("***%s", recvBuffer);
    //    //    printf("***%d", numRecv);
    //    //    if (numRecv < 0) {
    //    //        gfs_abort(ctx);
    //    //        break;
    //    //       // exit(1);
    //    //    }
    //    //    if (timer == 0) {
    //    //        memcpy(header, recvBuffer, numRecv);
    //    //    }
    //    //    else {
    //    //        strcat(header, recvBuffer);
    //    //    }
    //    //    timer = timer + 1;
    //    //} while (strstr(header, "\r\n\r\n") == NULL && timer < 1000);

    //    ////(*gfs)->handle(ctx, )
    //    //printf("CHECKING");
    //    //char scheme[100];
    //    //char method[100];
    //    //char path[1000];
    //    //memset(scheme, '\0', 100);
    //    //memset(method, '\0', 100);
    //    //memset(path, '\0', 1000);


    //    //sscanf(header, "%s %s %s\r\n\r\n", scheme, method, path);
    //    //(*ctx)->path = path; 

    //    //if ((strcmp(scheme, "GETFILE") == 0) && (strcmp(method, "GET") == 0) && (strncmp(path, "/", 1) == 0)) {
    //    //    if ((*gfs)->handle(ctx, (*ctx)->path, (*gfs)->arg) < 0) {
    //    //        if (gfs_sendheader(ctx, GF_ERROR, 0) < 0) {
    //    //            exit(1);
    //    //        }
    //    //    }
    //    //}
    //    //else {
    //    //    if (gfs_sendheader(ctx, GF_INVALID, 0) < 0) {
    //    //        exit(1);
    //    //    }
    //    //}
    //    gfs_abort(ctx);
    //    free(ctx);
    //}
    //close(serverSocket);
}


void gfserver_set_handlerarg(gfserver_t **gfs, void* arg){
    (*gfs)->arg = arg;
}

void gfserver_set_handler(gfserver_t **gfs, gfh_error_t (*handler)(gfcontext_t **, const char *, void*)){
    (*gfs)->handler = handler;
}

void gfserver_set_maxpending(gfserver_t **gfs, int max_npending){
    (*gfs)->maxPending = max_npending;
}

void gfserver_set_port(gfserver_t **gfs, unsigned short port){
    (*gfs)->serverPort = port;
}


