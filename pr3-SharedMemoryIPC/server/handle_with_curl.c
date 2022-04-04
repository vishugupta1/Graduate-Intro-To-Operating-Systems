

#include "gfserver.h"
#include "proxy-student.h"
#include <curl/curl.h>
#include <curl/easy.h>


#define BUFSIZE (128)


// reference -> https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
// https://curl.se/libcurl/c/getinmemory.html

struct memory {
	char* response;
	size_t size;
};

static size_t writer(void* data, size_t size, size_t nmemb, void* userp) {
	size_t realsize = size * nmemb;
	struct memory* mem = (struct memory*)userp;
	char* ptr = realloc(mem->response, mem->size + realsize + 1);
	if (ptr == NULL)
		return 0;
	mem->response = ptr;
	memcpy(&(mem->response[mem->size]), data, realsize);
	mem->size += realsize;
	mem->response[mem->size] = 0;
	return realsize;
}

/*
 * Replace with an implementation of handle_with_curl and any other
 * functions you may need.
 */
ssize_t handle_with_curl(gfcontext_t *ctx, const char *path, void* arg){
	(void) ctx;
	(void) path;
	(void) arg;

	// create url 
	char buffer[BUFSIZE];
	char* filedir = arg;
	//strcpy(buffer, filedir);
   // strcat(buffer, path);
	strncpy(buffer, filedir, BUFSIZE);
	strncat(buffer, path, BUFSIZE);
	printf("Path - %s\n", path);
	printf("Arg - %s\n", filedir);
	printf("Buffer - %s\n", buffer);

	if (strcmp(buffer, "") == 0) {
		printf("buffer equals empty string\n");
		return -1;
	}

	// create curl variables 
	size_t bytes_transferred;
	CURL* curl;
	CURLcode res;
	struct memory chunk;
	chunk.response = malloc(1);
	chunk.size = 0;

	// call curl functions 
	curl_global_init(CURL_GLOBAL_ALL);


	curl = curl_easy_init();
	//printf("Completed1\n");
	if (curl) {
		//curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, ctx->protocol);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
		curl_easy_setopt(curl, CURLOPT_URL, buffer); // url
		//printf("Completed1\n");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
		//printf("Completed1\n");
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
		//printf("Completed1\n");
		//curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
	}
	//printf("Completed1\n");
	if (res == CURLE_HTTP_RETURNED_ERROR) {
		//curl_easy_cleanup(curl);
		free(chunk.response);
		//curl_global_cleanup();
		return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
		//perror("curl gave problems\n");
		//return 0;
	}
	else if(res == CURLE_OK){
		gfs_sendheader(ctx, GF_OK, chunk.size);
		//printf("Completed2\n");
		bytes_transferred = 0;
		int i = -1;
	//	printf("CHUNK SIZE: %lu", chunk.size);
		while (bytes_transferred < chunk.size) {
			i++;
			//memset(buffer, 0, BUFSIZE);
			//strcpy(buffer, &chunk.response[i * BUFSIZE], BUFSIZE);
			//ssize_t bytes_left = chunk.size - bytes_transferred;
			//ssize_t length = (BUFSIZE > bytes_left) ? bytes_left : BUFSIZE;
		//	char* offset = chunk.response + bytes_transferred;
			//ssize_t n = gfs_send(ctx, offset, length); //<------- the error is here in the last iteration. 

			ssize_t n = gfs_send(ctx, &chunk.response[i * BUFSIZE], ((chunk.size - i*BUFSIZE) >= BUFSIZE) ? BUFSIZE : chunk.size - i*BUFSIZE); //<------- the error is here in the last iteration. 
															              // we are trying to access a element in the array that is out of bounds 
																		  // have to fix the math here 
			if (n < 0) {
				perror("No bytes received\n");
			}
			else {
			//	printf("bytes send %d", n);
				bytes_transferred += n;
			//	printf("total bytes transferred %lu", bytes_transferred);
			}
		}
		free(chunk.response);
		return bytes_transferred;
	}
	else{
		free(chunk.response);
		return EXIT_FAILURE;
	}
	
	
}


/*
 * We provide a dummy version of handle_with_file that invokes handle_with_curl
 * as a convenience for linking.  We recommend you simply modify the proxy to
 * call handle_with_curl directly.
 */
ssize_t handle_with_file(gfcontext_t *ctx, const char *path, void* arg){
	return handle_with_curl(ctx, path, arg);
}	
