#include <regex>
#include <mutex>
#include <vector>
#include <string>
#include <thread>
#include <cstdio>
#include <chrono>
#include <errno.h>
#include <csignal>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <sys/inotify.h>
#include <grpcpp/grpcpp.h>
#include <utime.h>

#include "src/dfs-utils.h"
#include "src/dfslibx-clientnode-p2.h"
#include "dfslib-shared-p2.h"
#include "dfslib-clientnode-p2.h"
#include "proto-src/dfs-service.grpc.pb.h"

using grpc::Status;
using grpc::Channel;
using grpc::StatusCode;
using grpc::ClientWriter;
using grpc::ClientReader;
using grpc::ClientContext;


extern dfs_log_level_e DFS_LOG_LEVEL;

//
// STUDENT INSTRUCTION:
//
// Change these "using" aliases to the specific
// message types you are using to indicate
// a file request and a listing of files from the server.
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



DFSClientNodeP2::DFSClientNodeP2() : DFSClientNode() {}
DFSClientNodeP2::~DFSClientNodeP2() {}

// DONE ----------------------------------------------------------------------------------------------------------------------------
grpc::StatusCode DFSClientNodeP2::RequestWriteAccess(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to obtain a write lock here when trying to store a file.
    // This method should request a write lock for the given file at the server,
    // so that the current client becomes the sole creator/writer. If the server
    // responds with a RESOURCE_EXHAUSTED response, the client should cancel
    // the current file storage
    //
    // The StatusCode response should be:
    //
    // OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //

    requestLockInput request;
    requestLockOutput response;
    ClientContext ctx;
    ctx.set_deadline(chrono::system_clock::now() + chrono::milliseconds(this->deadline_timeout));
    request.set_filename(filename);
    request.set_clientid(this->client_id);
    Status status = service_stub->requestLock(&ctx, request, &response);
    if(status.ok()){
        return grpc::OK;
    }
    else if (status.error_code() == 4){
        return grpc::DEADLINE_EXCEEDED;
    }
    else if (status.error_code() == 8){
        return grpc::RESOURCE_EXHAUSTED;
    }
    else{
        return grpc::CANCELLED;
    }
}

// DONE ----------------------------------------------------------------------------------------------------------------------------
grpc::StatusCode DFSClientNodeP2::Store(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to store a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation. However, you will
    // need to adjust this method to recognize when a file trying to be
    // stored is the same on the server (i.e. the ALREADY_EXISTS gRPC response).
    //
    // You will also need to add a request for a write lock before attempting to store.
    //
    // If the write lock request fails, you should return a status of RESOURCE_EXHAUSTED
    // and cancel the current operation.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::ALREADY_EXISTS - if the local cached file has not changed from the server version
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //

    storeFileInput request;
    storeFileOutput response;
    ClientContext ctx; 
    char buffer[4000];
    long fileLength = 0, readLength = 4000, bytesTransferred = 0;
    struct stat statbuf;
    fstream file;
    file.open(WrapPath(filename), ios::in | ios::binary);
    if(!file.is_open())
    {
        return grpc::CANCELLED;
    }
    uint32_t crc = dfs_file_checksum(WrapPath(filename), &this->crc_table);
    unique_ptr<ClientWriter<storeFileInput>> writer(service_stub->storeFile(&ctx, &response));
    stat(WrapPath(filename).c_str(), &statbuf);
    fileLength = (long)statbuf.st_size;
    if(fileLength < readLength){
        readLength = fileLength;
    }

    while(bytesTransferred < fileLength){
        if(fileLength - bytesTransferred < readLength){
            readLength = fileLength - bytesTransferred;
        }
        file.seekg(bytesTransferred);
        file.read(buffer, readLength);
        request.set_crc(crc);
        request.set_filename(filename);
        request.set_clientid(this->client_id);
        request.set_contentsize(readLength);
        request.set_content(buffer, readLength);
        writer->Write(request);
        bytesTransferred += readLength;
    }
    writer->WritesDone();
    Status status = writer->Finish();
    file.close();
    if(status.ok()){
        return grpc::OK;
    }
    else if (status.error_code() == 4){
        return grpc::DEADLINE_EXCEEDED;
    }
    else if (status.error_code() == 6){
        return grpc::ALREADY_EXISTS;
    }
    else{
        return grpc::CANCELLED;
    }

}

// DONE ----------------------------------------------------------------------------------------------------------------------------
grpc::StatusCode DFSClientNodeP2::Fetch(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to fetch a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation. However, you will
    // need to adjust this method to recognize when a file trying to be
    // fetched is the same on the client (i.e. the files do not differ
    // between the client and server and a fetch would be unnecessary.
    //
    // The StatusCode response should be:
    //
    // OK - if all went well
    // DEADLINE_EXCEEDED - if the deadline timeout occurs
    // NOT_FOUND - if the file cannot be found on the server
    // ALREADY_EXISTS - if the local cached file has not changed from the server version
    // CANCELLED otherwise
    //
    // Hint: You may want to match the mtime on local files to the server's mtime
    //

    fetchFileInput request;
    fetchFileOutput response;
    ClientContext ctx;
    fstream file;
    long bytesTransferred = 0;
    uint32_t crc = dfs_file_checksum(WrapPath(filename), &this->crc_table);
    //dfs_log(LL_DEBUG) << crc << "top";
    request.set_filename(filename);
    request.set_crc(crc);

    unique_ptr<ClientReader<fetchFileOutput>> reader(service_stub->fetchFile(&ctx, request));
    int flag = 0;
    while(reader->Read(&response)){
        if (response.contentsize() == 0) {
            printf("Not found");
            Status status = reader->Finish();
            return grpc::NOT_FOUND;
        }
        if (response.crc() == crc) {
            printf("Same crc");
            Status status = reader->Finish();
            return grpc::ALREADY_EXISTS;
        }
        if (flag == 0) {
            file.open(WrapPath(filename).c_str(), ios::out | ios::binary);
            flag = -1;
        }
        file.seekg(bytesTransferred);
        file.write(response.content().c_str(), (long)response.contentsize());
        bytesTransferred += (long)response.contentsize();
        //dfs_log(LL_DEBUG) << crc << "crc between";
    }
    Status status = reader->Finish();
    file.close();
    if(status.ok()){
        return grpc::OK;
    }
    else if (status.error_code() == 4){
        remove(WrapPath(filename).c_str());
        return grpc::DEADLINE_EXCEEDED;
    }
    else if (status.error_code() == 5) {
        remove(WrapPath(filename).c_str());
        return grpc::NOT_FOUND;
    }
    else if (status.error_code() == 6) {
        //dfs_log(LL_DEBUG) << crc << "client crc";
        return grpc::ALREADY_EXISTS;
    }
    else{
        remove(WrapPath(filename).c_str());
        return grpc::CANCELLED;
    }
}

// DONE ----------------------------------------------------------------------------------------------------------------------------
grpc::StatusCode DFSClientNodeP2::Delete(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to delete a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You will also need to add a request for a write lock before attempting to delete.
    //
    // If the write lock request fails, you should return a status of RESOURCE_EXHAUSTED
    // and cancel the current operation.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline tipmeout occurs
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //

    deleteFileInput request;
    deleteFileOutput response;
    ClientContext ctx;
    ctx.set_deadline(chrono::system_clock::now() + chrono::milliseconds(this->deadline_timeout));
    request.set_filename(filename);
    request.set_clientid(this->ClientId());
    Status status = service_stub->deleteFile(&ctx, request, &response);
    if(status.ok()){
        remove(WrapPath(filename).c_str());
        return grpc::OK;
    }
    else if (status.error_code() == 4){
        return grpc::DEADLINE_EXCEEDED;
    }
    else if (status.error_code() == 5) {
        return grpc::NOT_FOUND;
    }
    else if(status.error_code() == 8){
        return grpc::RESOURCE_EXHAUSTED;
    }
    else{
        return grpc::CANCELLED;
    }

}

// DONE ----------------------------------------------------------------------------------------------------------------------------
grpc::StatusCode DFSClientNodeP2::List(std::map<std::string,int>* file_map, bool display) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to list files here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation and add any additional
    // listing details that would be useful to your solution to the list response.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::CANCELLED otherwise
    //
    //

    listFilesInput request;
    listFilesOutput response;
    ClientContext ctx;
    unique_ptr<ClientReader<listFilesOutput>> reader(service_stub->listFiles(&ctx, request));
    ctx.set_deadline(chrono::system_clock::now() + chrono::milliseconds(this->deadline_timeout));
    while(reader->Read(&response)){
        file_map->insert(pair<string, int32_t>(response.filename(), response.mtime()));
    }
    Status status = reader->Finish();
    if (status.ok()) {
        return grpc::OK;
    }
    else if (status.error_code() == 4) {
        return grpc::DEADLINE_EXCEEDED;
    }
    else {
        return grpc::CANCELLED;
    }
}

// DONE ----------------------------------------------------------------------------------------------------------------------------
grpc::StatusCode DFSClientNodeP2::Stat(const std::string &filename, void* file_status) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to get the status of a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation and add any additional
    // status details that would be useful to your solution.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //

    ClientContext ctx;
    statusFileInput request;
    statusFileOutput response;
    request.set_filename(filename);
    ctx.set_deadline(chrono::system_clock::now() + chrono::milliseconds(this->deadline_timeout));
    Status status = service_stub->statusFile(&ctx, request, &response);
    if (status.ok()) {
        return grpc::OK;
    }
    else if (status.error_code() == 4) {
        return grpc::DEADLINE_EXCEEDED;
    }
    else if (status.error_code() == 5) {
        return grpc::NOT_FOUND;
    }
    else {
        return grpc::CANCELLED;
    }

}


/// DONE ----------------------------------------------------------------------------------------------------------------------------
void DFSClientNodeP2::InotifyWatcherCallback(std::function<void()> callback) {

    //
    // STUDENT INSTRUCTION:
    //
    // This method gets called each time inotify signals a change
    // to a file on the file system. That is every time a file is
    // modified or created.
    //
    // You may want to consider how this section will affect
    // concurrent actions between the inotify watcher and the
    // asynchronous callbacks associated with the server.
    //
    // The callback method shown must be called here, but you may surround it with
    // whatever structures you feel are necessary to ensure proper coordination
    // between the async and watcher threads.
    //
    // Hint: how can you prevent race conditions between this thread and
    // the async thread when a file event has been signaled?
    //

    lock_guard<mutex> lock(this->mux);
    callback();

}

// DONE ----------------------------------------------------------------------------------------------------------------------------
//
// STUDENT INSTRUCTION:
//
// This method handles the gRPC asynchronous callbacks from the server.
// We've provided the base structure for you, but you should review
// the hints provided in the STUDENT INSTRUCTION sections below
// in order to complete this method.
//
void DFSClientNodeP2::HandleCallbackList() {

    void* tag;
    bool ok = false;

    //
    // STUDENT INSTRUCTION:
    //
    // Add your file list synchronization code here.
    //
    // When the server responds to an asynchronous request for the CallbackList,
    // this method is called. You should then synchronize the
    // files between the server and the client based on the goals
    // described in the readme.
    //
    // In addition to synchronizing the files, you'll also need to ensure
    // that the async thread and the file watcher thread are cooperating. These
    // two threads could easily get into a race condition where both are trying
    // to write or fetch over top of each other. So, you'll need to determine
    // what type of locking/guarding is necessary to ensure the threads are
    // properly coordinated.
    //

    DIR *dir;
    struct dirent *ent;
    struct stat statbuf;
    //bool mount;

   // printf("Checking0");
  
    // Block until the next result is available in the completion queue.
    while (completion_queue.Next(&tag, &ok)) {
        {
            //
            // STUDENT INSTRUCTION:
            //
            // Consider adding a critical section or RAII style lock here
            //

            lock_guard<mutex> lock(mux);
            // The tag is the memory location of the call_data object
            AsyncClientData<FileListResponseType> *call_data = static_cast<AsyncClientData<FileListResponseType> *>(tag);

            dfs_log(LL_DEBUG2) << "Received completion queue callback";

            // Verify that the request was completed successfully. Note that "ok"
            // corresponds solely to the request for updates introduced by Finish().
            // GPR_ASSERT(ok);
            if (!ok) {
                dfs_log(LL_ERROR) << "Completion queue callback not ok.";
            }

            if (ok && call_data->status.ok()) {

                dfs_log(LL_DEBUG3) << "Handling async callback ";

                //
                // STUDENT INSTRUCTION:
                //
                // Add your handling of the asynchronous event calls here.
                // For example, based on the file listing returned from the server,
                // how should the client respond to this updated information?
                // Should it retrieve an updated version of the file?
                // Send an update to the server?
                // Do nothing?
                //

                auto& mtime_map = *call_data->reply.mutable_mtime();
                auto& crc_map = *call_data->reply.mutable_crc();
  
                for(auto& pair: crc_map){
                    string filename = pair.first;
                    int res = access(WrapPath(filename).c_str(), R_OK);
                    if(res < 0){
                        cout << "filename: " << filename << endl;
                        Fetch(filename);
                    }
                    else{
                        //printf("Checking1");
                        uint32_t crcClient = dfs_file_checksum(filename, &this->crc_table);
                        stat(WrapPath(filename).c_str(), &statbuf);
                        if(crc_map[filename] != crcClient){
                            if(mtime_map[filename] > statbuf.st_mtime){
                                Fetch(filename);
                            }
                            else{
                                cout << "File: "<< filename << " Server: " << crc_map[filename] << " Client: " << crcClient << endl << endl;
                                Store(filename);
                            }
                        }
                    }
                }

                //printf("Checking2");
                if ((dir = opendir(WrapPath("").c_str())) != NULL) {
                    while ((ent = readdir(dir)) != NULL) {
                        string filename = ent->d_name;
                        //uint32_t crcClient = dfs_file_checksum(filename, &this->crc_table);
                        if (crc_map.count(filename) == 0) {
                          /*  if (mount) {*/
                                printf("Checking4");
                                remove(WrapPath(filename).c_str());
                           //}
                          /*  else{
                                mount = true;
                                printf("Checking2");
                                Store(filename);
                            }*/
                            
                        }
                    }
                }

            } else {
                dfs_log(LL_ERROR) << "Status was not ok. Will try again in " << DFS_RESET_TIMEOUT << " milliseconds.";
                dfs_log(LL_ERROR) << call_data->status.error_message();
                std::this_thread::sleep_for(std::chrono::milliseconds(DFS_RESET_TIMEOUT));
            }

            // Once we're complete, deallocate the call_data object.
            delete call_data;

            //
            // STUDENT INSTRUCTION:
            //
            // Add any additional syncing/locking mechanisms you may need here

        }


        // Start the process over and wait for the next callback response
        dfs_log(LL_DEBUG3) << "Calling InitCallbackList";
        InitCallbackList();
    }
    free(dir);
    free(ent);
}

// DONE ----------------------------------------------------------------------------------------------------------------------------
/**
 * This method will start the callback request to the server, requesting
 * an update whenever the server sees that files have been modified.
 *
 * We're making use of a template function here, so that we can keep some
 * of the more intricate workings of the async process out of the way, and
 * give you a chance to focus more on the project's requirements.
 */
void DFSClientNodeP2::InitCallbackList() {
    CallbackList<FileRequestType, FileListResponseType>();
}

//
// STUDENT INSTRUCTION:
//
// Add any additional code you need to here
//


