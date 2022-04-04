#include "gfserver.h"
#include "cache-student.h"

#define BUFSIZE (256)

pthread_mutex_t mux = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

ssize_t handle_with_cache(gfcontext_t* ctx, const char* path, void* arg) {
	size_t file_len;
	size_t bytes_transferred;
	size_t write_len;
	ssize_t read_len;
	char data[3500];
	char buffer[BUFSIZE];
	int i = 0;
	void* ptr;
	pthread_mutex_lock(&(mux));
		while (steque_isempty(queue))
			pthread_cond_wait(&cond, &mux);
		ptr = steque_pop(queue);
	pthread_mutex_unlock(&(mux));
	i = *(int*)ptr;
	shm_object* shm = shm_create(i);
	bzero(data, 3500);
	strncat(data, path, 3500);
	sprintf(buffer, "%s %d", data, i);
	pthread_mutex_lock(&(mux));
	mqueue_write(buffer);
	pthread_mutex_unlock(&(mux));
	shm_wait_semaphore(shm);
	file_len = shm_read_fileLength(shm);
	if (file_len == 0) {
		munmap(shm, segmentSize);
		pthread_mutex_lock(&(mux));
		steque_enqueue(queue, ptr);
		pthread_mutex_unlock(&(mux));
		pthread_cond_signal(&cond);
		ptr = NULL;
		return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, file_len);
	}
	gfs_sendheader(ctx, GF_OK, file_len);
	bytes_transferred = 0;
	int k = 0;
	while (bytes_transferred < file_len) {
		k = file_len - bytes_transferred; 
		shm_wait_semaphore_r(shm);
		bzero(data, 3500);
		read_len = shm_read_data(shm, data);
		shm_post_semaphore_w(shm);
		if (read_len < 0) {
			fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len);
			return SERVER_FAILURE;
		}

		if(k < 3499)
			write_len = gfs_send(ctx, data, k); 
		else
			write_len = gfs_send(ctx, data, read_len);

		bytes_transferred += write_len;
	}
	pthread_mutex_lock(&(mux));
	munmap(shm, segmentSize);
	steque_enqueue(queue, ptr);
	pthread_mutex_unlock(&(mux));
	pthread_cond_signal(&cond);
	ptr = NULL;
	return bytes_transferred;
}









//
//
//	if( 0 > (fildes = open(buffer, O_RDONLY))){
//		if (errno == ENOENT)
//			/* If the file just wasn't found, then send FILE_NOT_FOUND code*/ 
//			return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
//		else
//			/* Otherwise, it must have been a server error. gfserver library will handle*/ 
//			return SERVER_FAILURE;
//	}
//
//	/* Calculating the file size */
//	if (0 > fstat(fildes, &statbuf)) {
//		return SERVER_FAILURE;
//	}
//
//	file_len = (size_t) statbuf.st_size;
//
//	gfs_sendheader(ctx, GF_OK, file_len);
//
//	/* Sending the file contents chunk by chunk. */
//	bytes_transferred = 0;
//	while(bytes_transferred < file_len){
//		read_len = read(fildes, buffer, BUFSIZE);
//		if (read_len <= 0){
//			fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
//			return SERVER_FAILURE;
//		}
//		write_len = gfs_send(ctx, buffer, read_len);
//		if (write_len != read_len){
//			fprintf(stderr, "handle_with_file write error");
//			return SERVER_FAILURE;
//		}
//		bytes_transferred += write_len;
//	}
//
//	return bytes_transferred;
//}
//
//









/*


typedef struct{
	pthread_mutex_t mutex;
	steque_t queue;

} shm_data_struct *shm_data_struct_t;


typedef struct stequeitem{
	const char* path;
} stequeitem;


// this handle should get it from cache (shared memory) first
size_t handle_with_cache(gfcontext_t *ctx, const char *path, void* arg){

	char *data_dir = arg;
	char buffer[BUFSIZE];
	strncpy(buffer,data_dir, BUFSIZE);
	strncat(buffer,path, BUFSIZE);


	mqueue_write()



	// Create shared mutex to input items into buffer for cache to recieve
	// Both cache and client will have mutliple threads
	// Reference -> https://gatech.instructure.com/courses/205090/pages/13-pthreads-sync-for-ipc?module_item_id=1887286
	// Initialization:
	seg = shmget(ftok(buffer, 120), 1024, IPC_CREATE|IPC_EXCL));
	shm_address = shmat(seg, (void*) 0, 0);
	shm_ptr = (shm_data_struct_t)shm_address;
	pthread_mutexattr_t(&m_attr);
	pthread_mutex_attr_set_pshared(&m_attr, PTHREAD_PREOCESS_SHARED);
	pthread_mutex_init(&shm_ptr.mutex, &m_attr);





	pthread_mutex_lock(&shm_ptr.mutex);
		stequeitem* item = (stequeitem*)malloc(sizeof(stequeitem));
		item->path = buffer;
		steque_enqueue(queue, item);
	pthread_mutex_unlock(&shm_ptr.mutex)



}













*/