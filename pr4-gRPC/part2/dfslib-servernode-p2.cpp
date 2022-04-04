#include <map>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <grpcpp/grpcpp.h>

#include "proto-src/dfs-service.grpc.pb.h"
#include "src/dfslibx-call-data.h"
#include "src/dfslibx-service-runner.h"
#include "dfslib-shared-p2.h"
#include "dfslib-servernode-p2.h"

using grpc::Status;
using grpc::Server;
using grpc::StatusCode;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ServerContext;
using grpc::ServerBuilder;



//
// STUDENT INSTRUCTION:
//
// Change these "using" aliases to the specific
// message types you are using in your `dfs-service.proto` file
// to indicate a file request and a listing of files from the server
//
using FileRequestType = dfs_service::CallbackListInput;
using FileListResponseType = dfs_service::CallbackListOutput;

using dfs_service::DFSService;
using dfs_service::storeFileInput;
using dfs_service::storeFileOutput;
using dfs_service::fetchFileInput;
using dfs_service::fetchFileOutput;
using dfs_service::listFilesInput;
using dfs_service::listFilesOutput;
using dfs_service::statusFileInput;
using dfs_service::statusFileOutput;
using dfs_service::requestLockInput;
using dfs_service::requestLockOutput;
using dfs_service::deleteFileInput;
using dfs_service::deleteFileOutput;
using namespace std;

extern dfs_log_level_e DFS_LOG_LEVEL;

//
// STUDENT INSTRUCTION:
//
// As with Part 1, the DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in your `dfs-service.proto` file.
//
// You may start with your Part 1 implementations of each service method.
//
// Elements to consider for Part 2:
//
// - How will you implement the write lock at the server level?
// - How will you keep track of which client has a write lock for a file?
//      - Note that we've provided a preset client_id in DFSClientNode that generates
//        a client id for you. You can pass that to the server to identify the current client.
// - How will you release the write lock?
// - How will you handle a store request for a client that doesn't have a write lock?
// - When matching files to determine similarity, you should use the `file_checksum` method we've provided.
//      - Both the client and server have a pre-made `crc_table` variable to speed things up.
//      - Use the `file_checksum` method to compare two files, similar to the following:
//
//          std::uint32_t server_crc = dfs_file_checksum(filepath, &this->crc_table);
//
//      - Hint: as the crc checksum is a simple integer, you can pass it around inside your message types.
//
class DFSServiceImpl final :
    public DFSService::WithAsyncMethod_CallbackList<DFSService::Service>,
        public DFSCallDataManager<FileRequestType , FileListResponseType> {

private:

    /** The runner service used to start the service and manage asynchronicity **/
    DFSServiceRunner<FileRequestType, FileListResponseType> runner;

    /** The mount path for the server **/
    std::string mount_path;

    /** Mutex for managing the queue requests **/
    std::mutex queue_mutex;

    /** The vector of queued tags used to manage asynchronous requests **/
    std::vector<QueueRequest<FileRequestType, FileListResponseType>> queued_tags;


    /**
     * Prepend the mount path to the filename.
     *
     * @param filepath
     * @return
     */
    const std::string WrapPath(const std::string &filepath) {
        return this->mount_path + filepath;
    }

    /** CRC Table kept in memory for faster calculations **/
    CRC::Table<std::uint32_t, 32> crc_table;

    std::map<string, string> lockList;
    std::map<string, uint32_t> crc;

public:

    DFSServiceImpl(const std::string& mount_path, const std::string& server_address, int num_async_threads):
        mount_path(mount_path), crc_table(CRC::CRC_32()) {

        this->runner.SetService(this);
        this->runner.SetAddress(server_address);
        this->runner.SetNumThreads(num_async_threads);
        this->runner.SetQueuedRequestsCallback([&]{ this->ProcessQueuedRequests(); });

    }

    ~DFSServiceImpl() {
        this->runner.Shutdown();
    }

    void Run() {
        this->runner.Run();
    }

    /**
     * Request callback for asynchronous requests
     *
     * This method is called by the DFSCallData class during
     * an asynchronous request call from the client.
     *
     * Students should not need to adjust this.
     *
     * @param context
     * @param request
     * @param response
     * @param cq
     * @param tag
     */
    void RequestCallback(grpc::ServerContext* context,
                         FileRequestType* request,
                         grpc::ServerAsyncResponseWriter<FileListResponseType>* response,
                         grpc::ServerCompletionQueue* cq,
                         void* tag) {

        lock_guard<mutex> lock(queue_mutex);
        this->queued_tags.emplace_back(context, request, response, cq, tag);
    }

    // DONE ----------------------------------------------------------------------------------------------------------------------------
    /**
     * Process a callback request
     *
     * This method is called by the DFSCallData class when
     * a requested callback can be processed. You should use this method
     * to manage the CallbackList RPC call and respond as needed.
     *
     * See the STUDENT INSTRUCTION for more details.
     *
     * @param context
     * @param request
     * @param response
     */
    void ProcessCallback(ServerContext* context, FileRequestType* request, FileListResponseType* response) {

        //
        // STUDENT INSTRUCTION:
        //
        // You should add your code here to respond to any CallbackList requests from a client.
        // This function is called each time an asynchronous request is made from the client.
        //
        // The client should receive a list of files or modifications that represent the changes this service
        // is aware of. The client will then need to make the appropriate calls based on those changes.
        //

        DIR* dir;
        struct dirent* ent;
        struct stat statbuf;

        if ((dir = opendir(WrapPath("").c_str())) != NULL) {
            while ((ent = readdir(dir)) != NULL) {
                stat(WrapPath(ent->d_name).c_str(), &statbuf);
                (*response->mutable_mtime())[ent->d_name] = statbuf.st_mtime;
                (*response->mutable_crc())[ent->d_name] = dfs_file_checksum(WrapPath(ent->d_name), &this->crc_table);
            }
        }
        free(dir);
        free(ent);
    }




    // DONE ----------------------------------------------------------------------------------------------------------------------------
    /**
     * Processes the queued requests in the queue thread
     */
    void ProcessQueuedRequests() {
        while(true) {

            //
            // STUDENT INSTRUCTION:
            //
            // You should add any synchronization mechanisms you may need here in
            // addition to the queue management. For example, modified files checks.
            //
            // Note: you will need to leave the basic queue structure as-is, but you
            // may add any additional code you feel is necessary.
            //


            // Guarded section for queue
            {
                dfs_log(LL_DEBUG2) << "Waiting for queue guard";
                lock_guard<mutex> lock(queue_mutex);


                for(QueueRequest<FileRequestType, FileListResponseType>& queue_request : this->queued_tags) {
                    this->RequestCallbackList(queue_request.context, queue_request.request,
                        queue_request.response, queue_request.cq, queue_request.cq, queue_request.tag);
                    queue_request.finished = true;
                }

                // any finished tags first
                this->queued_tags.erase(std::remove_if(
                    this->queued_tags.begin(),
                    this->queued_tags.end(),
                    [](QueueRequest<FileRequestType, FileListResponseType>& queue_request) { return queue_request.finished; }
                ), this->queued_tags.end());

            }
        }
    }

    // DONE ----------------------------------------------------------------------------------------------------------------------------
    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // the implementations of your rpc protocol methods.
    //

    Status storeFile(::grpc::ServerContext* context,::grpc::ServerReader<::dfs_service::storeFileInput>* reader, ::dfs_service::storeFileOutput* response) override{
        storeFileInput request;
        fstream file;
        int flag = 1;
        //long readLength;
        long bytesTransferred = 0;
        while(reader->Read(&request)){
            if (context->IsCancelled()) {
                return Status(StatusCode::DEADLINE_EXCEEDED, "");
            }
            if(flag == 1){
                Status lockStatus = lock(request.filename(), request.clientid());
                if (lockStatus.error_code() == 8) {
                    return Status(grpc::RESOURCE_EXHAUSTED, "");
                }
                uint32_t crc = dfs_file_checksum(WrapPath(request.filename()), &this->crc_table);
                if (request.crc() == crc) {
                    unlock(request.filename());
                    return Status(grpc::ALREADY_EXISTS, "");
                }
                file.open(WrapPath(request.filename()), ios::out | ios::binary);
                flag = 0;
            }
            file.seekg(bytesTransferred);
            file.write(request.content().c_str(), request.contentsize());
            bytesTransferred += request.contentsize();
        }
        file.close();
        unlock(request.filename());
        return Status(grpc::OK, "");
    }

    // DONE ----------------------------------------------------------------------------------------------------------------------------
    Status fetchFile(::grpc::ServerContext* context, const ::dfs_service::fetchFileInput* request, ::grpc::ServerWriter< ::dfs_service::fetchFileOutput>* writer) override {  
        fetchFileOutput response;
        fstream file;
        file.open(WrapPath(request->filename()), ios::in | ios::binary);
        long fileLength = 0, readLength = 4000, bytesTransferred = 0;
        struct stat statbuf;
        if (!file.is_open()) {
            //printf("file not found");
            response.set_contentsize(0);
            writer->Write(response);
            return Status(grpc::NOT_FOUND, "");
        }
        uint32_t crc = dfs_file_checksum(WrapPath(request->filename()), &this->crc_table);
        //dfs_log(LL_DEBUG) << crc << "server crc";
        //dfs_log(LL_DEBUG) << request->crc() << "server from client crc";

        stat(WrapPath(request->filename()).c_str(), &statbuf);
        fileLength = (long)statbuf.st_size;
        char buffer[readLength];
        if (fileLength < readLength) {
            readLength = fileLength;
        }
        response.set_crc(crc);
        if (request->crc() == crc) {
            //printf("same crc");
            file.close();
            writer->Write(response);
            return Status(grpc::ALREADY_EXISTS, "");
        }
        while(bytesTransferred < fileLength){
            if (context->IsCancelled()) {
                file.close();
                return Status(grpc::DEADLINE_EXCEEDED, "");
            }
            if(fileLength - bytesTransferred < readLength){
                readLength = fileLength - bytesTransferred;
            }
            file.seekg(bytesTransferred);
            file.read(buffer, readLength);
            response.set_contentsize(readLength);
            response.set_content(buffer, readLength);
            bytesTransferred += readLength;
            writer->Write(response);
        }
        file.close();
        return Status(grpc::OK, "");
    }

    // DONE ----------------------------------------------------------------------------------------------------------------------------
    Status listFiles(::grpc::ServerContext* context, const ::dfs_service::listFilesInput* request, ::grpc::ServerWriter< ::dfs_service::listFilesOutput>* writer){

        DIR* dir;
        struct dirent* ent;
        struct stat statbuf;
        listFilesOutput response;

        if((dir = opendir(WrapPath("").c_str())) != NULL){
            while((ent = readdir(dir)) != NULL){
                if (context->IsCancelled()) {
                    return Status(grpc::DEADLINE_EXCEEDED, "");
                }
                stat(WrapPath(ent->d_name).c_str(), &statbuf);
                response.set_filename(ent->d_name);
                response.set_mtime(statbuf.st_mtime);
                writer->Write(response);   
            }
        }
        return Status(grpc::OK, "");
    }

    // DONE ----------------------------------------------------------------------------------------------------------------------------
    Status statusFile(::grpc::ServerContext* context, const ::dfs_service::statusFileInput* request, ::dfs_service::statusFileOutput* response){
        fstream file; 
        file.open(WrapPath(request->filename()));
        if (!file.is_open()) {
            return Status(grpc::NOT_FOUND, "");
        }
        struct stat statbuf;
        stat(WrapPath(request->filename()).c_str(), &statbuf);
        uint32_t crc = dfs_file_checksum(WrapPath(request->filename().c_str()), &this->crc_table);
        response->set_filename(request->filename());
        response->set_filelength(statbuf.st_size);
        response->set_crc(crc);
        response->set_mtime(statbuf.st_mtime);
        response->set_ctime(statbuf.st_ctime);
        return Status(grpc::OK, "");
    }

    // DONE ----------------------------------------------------------------------------------------------------------------------------
    // Lock Operations
    Status requestLock(::grpc::ServerContext* context, const ::dfs_service::requestLockInput* request, ::dfs_service::requestLockOutput* response){
        return lock(request->filename(), request->clientid());
    }

    Status lock(string fileName, string clientID) {
        if (this->lockList.count(fileName) == 0 || this->lockList[fileName].compare(clientID) == 0) {
            this->lockList[fileName] = clientID;
            return Status(grpc::OK, "");
        }
        return Status(grpc::RESOURCE_EXHAUSTED, "\nThis client already holds the lock to this file.\n"); 
    }

    Status unlock(string fileName) {
        if (lockList.count(fileName) == 0) {
            return Status(grpc::NOT_FOUND, "\nFile does not exist in directory.\n");
        }
        this->lockList.erase(fileName);
        return Status(grpc::OK, "");
    }



    //// DONE ----------------------------------------------------------------------------------------------------------------------------
    //Status CallbackList(::grpc::ServerContext* context, const ::dfs_service::CallbackListInput* request, ::grpc::ServerWriter< ::dfs_service::CallbackListOutput>* writer){}


    // DONE ----------------------------------------------------------------------------------------------------------------------------
    Status deleteFile(::grpc::ServerContext* context, const ::dfs_service::deleteFileInput* request, ::dfs_service::deleteFileOutput* response) override {
        if (context->IsCancelled()) {
            return Status(StatusCode::DEADLINE_EXCEEDED, "");
        }
        int flag = remove(WrapPath(request->filename()).c_str());
        if (flag == -1) {
            return Status(grpc::NOT_FOUND, "");
        }
        Status lockStatus = lock(request->filename(), request->clientid());
        if (lockStatus.error_code() == 8) {
            return Status(grpc::RESOURCE_EXHAUSTED, "");
        }
        unlock(request->filename());
        return Status(grpc::OK, "");
    }
};

//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure. You may add additional methods or change these slightly
// to add additional startup/shutdown routines inside, but be aware that
// the basic structure should stay the same as the testing environment
// will be expected this structure.
//
/**
 * The main server node constructor
 *
 * @param mount_path
 */
DFSServerNode::DFSServerNode(const std::string &server_address,
        const std::string &mount_path,
        int num_async_threads,
        std::function<void()> callback) :
        server_address(server_address),
        mount_path(mount_path),
        num_async_threads(num_async_threads),
        grader_callback(callback) {}
/**
 * Server shutdown
 */
DFSServerNode::~DFSServerNode() noexcept {
    dfs_log(LL_SYSINFO) << "DFSServerNode shutting down";
}

/**
 * Start the DFSServerNode server
 */
void DFSServerNode::Start() {
    DFSServiceImpl service(this->mount_path, this->server_address, this->num_async_threads);


    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    service.Run();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional definitions here
//
