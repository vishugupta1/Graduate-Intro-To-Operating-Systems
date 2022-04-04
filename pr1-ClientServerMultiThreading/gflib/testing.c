#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


int recieveData(gfr)




















int main() {

    char str[100] = "GETFILE OK 1234\r\n\r\nefefwefwef";
    char* token;
    token = strtok(str, " ");
    char* scheme;
    char* status;
    strcpy

    int i = 0;
    while (token != NULL) {
        if (i == 0) {
            if (strcmp(token, "GETFILE")) {

            }

        }
        if (i == 1) {
            if (strcmp(token, "OK")) {
                printf("%s", "Success");
            }
        }

        printf(" %s\n", token);

        token = strtok(NULL, " ");
    }

    return(0);







    //const char s[5] = "\r\n\r\n";
    char* token;

    /* get the first token */
    token = strtok(str, "\r\n\r\n");
    printf("%s\n", token);

    char buffer[100];
    strncpy(buffer, token, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    printf('%s', &buffer);

    //char str[]
    //char* tokenHeader;





    /* walk through other tokens */
    while (token != NULL) {
        printf(" %s\n", token);

        token = strtok(NULL, "\r\n\r\n");
    }

    return(0);
}


