#include <regex>
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

#include "dfslib-shared-p1.h"
#include "dfslib-clientnode-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"

using grpc::Status;
using grpc::Channel;
using grpc::StatusCode;
using grpc::ClientWriter;
using grpc::ClientReader;
using grpc::ClientContext;

//
// STUDENT INSTRUCTION:
//
// You may want to add aliases to your namespaced service methods here.
// All of the methods will be under the `dfs_service` namespace.
//
// For example, if you have a method named MyMethod, add
// the following:
//
//      using dfs_service::MyMethod
//

using dfs_service::DFSService;
using dfs_service::File;
using dfs_service::listFiles;
using dfs_service::statusFile;
using dfs_service::FileName;
using dfs_service::ListFilesResponse;
using dfs_service::FileReply;
using namespace std;

// https://grpc.io/blog/deadlines/
// https://grpc.io/docs/languages/cpp/basics/



DFSClientNodeP1::DFSClientNodeP1() : DFSClientNode() {}

DFSClientNodeP1::~DFSClientNodeP1() noexcept {}

StatusCode DFSClientNodeP1::Store(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to store a file here. This method should
    // connect to your gRPC service implementation method
    // that can accept and store a file.
    //
    // When working with files in gRPC you'll need to stream
    // the file contents, so consider the use of gRPC's ClientWriter.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the client
    // StatusCode::CANCELLED otherwise
    //
    //

    File request;
    FileReply response;
    ClientContext ctx;
    long readLength = 0, fileLength = 0, bytesTransferred = 0;
    readLength = 4000;
    char buffer[readLength];
    struct stat statBuf;
    fstream file;
    ctx.set_deadline(chrono::system_clock::now() + chrono::milliseconds(this->deadline_timeout));
    file.open(WrapPath(filename), ios::in | ios::binary);
    if (!file.is_open()) {
        return grpc::NOT_FOUND;
    }
    unique_ptr<ClientWriter<File>> writer(service_stub->uploadFile(&ctx, &response)); 
    stat(WrapPath(filename).c_str(), &statBuf);
    fileLength = (long)statBuf.st_size;
    if (fileLength < readLength) {
        readLength = fileLength;
    }
    printf("working");
    while (bytesTransferred < fileLength) {
        if (fileLength - bytesTransferred < readLength) {
            readLength = fileLength - bytesTransferred;
        }
        file.seekg(bytesTransferred);
        file.read(buffer, readLength);
        request.set_filename(filename);
        request.set_contentsize(readLength);
        request.set_content(buffer, readLength);
        writer->Write(request);
        bytesTransferred += readLength;
    }
    printf("working");
    file.close();
    writer->WritesDone();
    Status status = writer->Finish();
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
 

StatusCode DFSClientNodeP1::Fetch(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to fetch a file here. This method should
    // connect to your gRPC service implementation method
    // that can accept a file request and return the contents
    // of a file from the service.
    //
    // As with the store function, you'll need to stream the
    // contents, so consider the use of gRPC's ClientReader.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //

    FileName request;
    File response;
    // FileName requestStatus;
    //statusFile responseStatus;

    ClientContext ctx;

    fstream file;
    long readLength = 0;
    long bytesTransferred = 0;
    ctx.set_deadline(chrono::system_clock::now() + chrono::milliseconds(this->deadline_timeout));
    file.open(WrapPath(filename), ios::out | ios::binary);

    request.set_filename(filename);
    // requestStatus.set_filename(filename);

    unique_ptr<ClientReader<File>> reader(service_stub->downloadFile(&ctx, request));
    printf("working");
    while (reader->Read(&response)) {
        readLength = response.contentsize();
        file.seekg(bytesTransferred);
        file.write(response.content().c_str(), readLength);
        bytesTransferred += readLength;
    }
    printf("working");
    Status status = reader->Finish();
    file.close();
    if (status.ok()) {
        return grpc::OK;
    }
    else if(status.error_code() == 5) {
        remove(WrapPath(filename).c_str());
        return grpc::NOT_FOUND;
    }
    else if (status.error_code() == 4) {
        return grpc::DEADLINE_EXCEEDED;
    }
    else {
        return grpc::CANCELLED;
    }


}

StatusCode DFSClientNodeP1::Delete(const std::string& filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to delete a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //

    FileName request;
    FileReply response;
    ClientContext ctx;
    ctx.set_deadline(chrono::system_clock::now() + chrono::milliseconds(deadline_timeout));
    request.set_filename(filename);
    Status status = service_stub->destroyFile(&ctx, request, &response);
    if (status.ok()) {
        return grpc::OK;
    }
    else if (status.error_code() == 5) {
        return grpc::NOT_FOUND;
    }
    else if (status.error_code() == 4) {
        return grpc::DEADLINE_EXCEEDED;
    }
    else {
        return grpc::CANCELLED;
    }

}

StatusCode DFSClientNodeP1::List(std::map<std::string,int>* file_map, bool display) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to list all files here. This method
    // should connect to your service's list method and return
    // a list of files using the message type you created.
    //
    // The file_map parameter is a simple map of files. You should fill
    // the file_map with the list of files you receive with keys as the
    // file name and values as the modified time (mtime) of the file
    // received from the server.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::CANCELLED otherwise
    //
    //

    listFiles request;
    ListFilesResponse response;
    ClientContext ctx;
    ctx.set_deadline(chrono::system_clock::now() + chrono::milliseconds(deadline_timeout));
    Status status = service_stub->retrieveFiles(&ctx, request, &response);
    if (status.ok()) {
        for (auto& pair : response.filelist()) {
            (*file_map)[pair.first] = pair.second;
        }
        return grpc::OK;
    }
    else if (status.error_code() == 4) {
        return grpc::DEADLINE_EXCEEDED;
    }
    else {
        return grpc::CANCELLED;
    }
}

StatusCode DFSClientNodeP1::Stat(const std::string &filename, void* file_status) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to get the status of a file here. This method should
    // retrieve the status of a file on the server. Note that you won't be
    // tested on this method, but you will most likely find that you need
    // a way to get the status of a file in order to synchronize later.
    //
    // The status might include data such as name, size, mtime, crc, etc.
    //
    // The file_status is left as a void* so that you can use it to pass
    // a message type that you defined. For instance, may want to use that message
    // type after calling Stat from another method.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //

    FileName request;
    statusFile response;
    request.set_filename(filename);
    ClientContext ctx;
    ctx.set_deadline(chrono::system_clock::now() + chrono::milliseconds(deadline_timeout));
    Status status = service_stub->statusOfFile(&ctx, request, &response);
    if (status.ok()) {
        return grpc::OK;
    }
    else if (status.error_code() == 5) {
        return grpc::NOT_FOUND;
    }
    else if (status.error_code() == 4) {
        return grpc::DEADLINE_EXCEEDED;
    }
    else {
        return grpc::CANCELLED;
    }


}

//
// STUDENT INSTRUCTION:
//
// Add your additional code here, including
// implementations of your client methods
//

