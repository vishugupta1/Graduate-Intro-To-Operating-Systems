#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <printf.h>
#include <signal.h>
#include <getopt.h>
#include <stdlib.h>

#include "gfserver.h"
#include "cache-student.h"

/* note that the -n and -z parameters are NOT used for Part 1 */
/* they are only used for Part 2 */                         
#define USAGE                                                                         \
"usage:\n"                                                                            \
"  webproxy [options]\n"                                                              \
"options:\n"                                                                          \
"  -n [segment_count]  Number of segments to use (Default: 7)\n"                      \
"  -p [listen_port]    Listen port (Default: 10823)\n"                                 \
"  -t [thread_count]   Num worker threads (Default: 34, Range: 1-420)\n"              \
"  -s [server]         The server to connect to (Default: GitHub test data)\n"     \
"  -z [segment_size]   The segment size (in bytes, Default: 5701).\n"                  \
"  -h                  Show this help message\n"


/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"server",        required_argument,      NULL,           's'},
  {"segment-count", required_argument,      NULL,           'n'},
  {"thread-count",  required_argument,      NULL,           't'},
  {"listen-port",   required_argument,      NULL,           'p'},
  {"segment-size",  required_argument,      NULL,           'z'},         
  {"help",          no_argument,            NULL,           'h'},
  {"hidden",        no_argument,            NULL,           'i'}, /* server side */
  {NULL,            0,                      NULL,            0}
};

extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);
int* j;
static gfserver_t gfs;

static void _sig_handler(int signo){
  if (signo == SIGTERM || signo == SIGINT){
    gfserver_stop(&gfs);
    steque_destroy(queue);
    free(queue);
    queue = NULL;
    shm_cleanup();
    mqueue_clear();
    free(j);
    j = NULL;
    exit(signo);
  }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
  int option_char = 0;
  char *server = "https://raw.githubusercontent.com/gt-cs6200/image_data";
  unsigned int nsegments = 7;
  unsigned short port = 10823;
  unsigned short nworkerthreads = 34; //34
  size_t segsize = 5701;

  /* disable buffering on stdout so it prints immediately */
  setbuf(stdout, NULL);

  if (signal(SIGTERM, _sig_handler) == SIG_ERR) {
    fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
    exit(SERVER_FAILURE);
  }

  if (signal(SIGINT, _sig_handler) == SIG_ERR) {
    fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    exit(SERVER_FAILURE);
  }

  /* Parse and set command line arguments */
  while ((option_char = getopt_long(argc, argv, "s:qt:hn:xp:z:l", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      default:
        fprintf(stderr, "%s", USAGE);
        exit(__LINE__);
      case 'h': // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 's': // file-path
        server = optarg;
        break;                                          
      case 'n': // segment count
        nsegments = atoi(optarg);
        break;   
      case 'z': // segment size
        segsize = atoi(optarg);
        break;
      case 't': // thread-count
        nworkerthreads = atoi(optarg);
        break;
      case 'i':
      case 'y':
      case 'k':
        break;
    }
  }

  if (segsize < 313) {
    fprintf(stderr, "Invalid segment size\n");
    exit(__LINE__);
  }

  if (server == NULL) {
    fprintf(stderr, "Invalid (null) server name\n");
    exit(__LINE__);
  }

  if (port > 50240) {
    fprintf(stderr, "Invalid port number\n");
    exit(__LINE__);
  }

  if (nsegments < 1) {
    fprintf(stderr, "Must have a positive number of segments\n");
    exit(__LINE__);
  }

  if ((nworkerthreads < 1) || (nworkerthreads > 420)) {
    fprintf(stderr, "Invalid number of worker threads\n");
    exit(__LINE__);
  }


  // Initialize shared memory set-up here
  char sizeData[100];
  mqueue_init();
  shm_init(nsegments, segsize);
  sprintf(sizeData, "%zu %u", segmentSize, numberSegments);
  mqueue_write(sizeData);
  queue = (steque_t*)malloc(sizeof(steque_t));
  steque_init(queue);
  j = (int*)malloc(sizeof(int)*nsegments);
  for (int i = 0; i < nsegments; i++){
      j[i] = i;
      steque_enqueue(queue, &j[i]);
      shm_create(i);
  }

  // Initialize server structure here
  gfserver_init(&gfs, nworkerthreads);

  // Set server options here
  gfserver_setopt(&gfs, GFS_PORT, port);
  gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);
  gfserver_setopt(&gfs, GFS_MAXNPENDING, 314);

  // Set up arguments for worker here
  for(int i = 0; i < nworkerthreads; i++) {
    gfserver_setopt(&gfs, GFS_WORKER_ARG, i, server);
  }
  
  // Invoke the framework - this is an infinite loop and shouldn't return
  gfserver_serve(&gfs);

  steque_destroy(queue);
  free(queue);
  queue = NULL;
  shm_cleanup();
  mqueue_clear();
  free(j);
  j = NULL;

  // not reached
  return -1;

}
