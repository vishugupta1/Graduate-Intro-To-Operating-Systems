#include <map>
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

#include "src/dfs-utils.h"
#include "dfslib-shared-p1.h"
#include "dfslib-servernode-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"

using grpc::Status;
using grpc::Server;
using grpc::StatusCode;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ServerContext;
using grpc::ServerBuilder;


using dfs_service::DFSService;
using dfs_service::File;
using dfs_service::listFiles;
using dfs_service::statusFile;
using dfs_service::FileName;
using dfs_service::ListFilesResponse;
using dfs_service::FileReply;
using namespace std;


//
// STUDENT INSTRUCTION:
//
// DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in the `dfs-service.proto` file.
//
// You should add your definition overrides here for the specific
// methods that you created for your GRPC service protocol. The
// gRPC tutorial described in the readme is a good place to get started
// when trying to understand how to implement this class.
//
// The method signatures generated can be found in `proto-src/dfs-service.grpc.pb.h` file.
//
// Look for the following section:
//
//      class Service : public ::grpc::Service {
//
// The methods returning grpc::Status are the methods you'll want to override.
//
// In C++, you'll want to use the `override` directive as well. For example,
// if you have a service method named MyMethod that takes a MyMessageType
// and a ServerWriter, you'll want to override it similar to the following:
//
//      Status MyMethod(ServerContext* context,
//                      const MyMessageType* request,
//                      ServerWriter<MySegmentType> *writer) override {
//
//          /** code implementation here **/
//      }
//
class DFSServiceImpl final : public DFSService::Service {

private:

    /** The mount path for the server **/
    std::string mount_path;

    /**
     * Prepend the mount path to the filename.
     *
     * @param filepath
     * @return
     */
    const std::string WrapPath(const std::string &filepath) {
        return this->mount_path + filepath;
    }


public:

    DFSServiceImpl(const std::string &mount_path): mount_path(mount_path) {
    }

    ~DFSServiceImpl() {}

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // implementations of your protocol service methods
    //


    Status uploadFile(ServerContext* ctx, ServerReader<File>* reader, FileReply* response) override {
        File request;
        fstream file;
        long readLength = 0;
        long bytesTransferred = 0;
        int flag = 0;
        while (reader->Read(&request)) {
            if (ctx->IsCancelled()) {
                return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline exceeded or Client cancelled, abandoning.");
            }
            if (flag == 0) {
                file.open(WrapPath(request.filename()), ios::out | ios::binary);
                flag = 1;
            }
            readLength = request.contentsize();
            file.seekg(bytesTransferred);
            file.write(request.content().c_str(), readLength);
            bytesTransferred += readLength;

        }
        return Status(grpc::OK, "");
    }

    Status downloadFile(ServerContext* ctx, const FileName* request, ServerWriter<File>* writer) override {
        string filename = request->filename();
        fstream file;
        File response;
        file.open(WrapPath(filename), ios::in | ios::binary);
        if (!file.is_open())
            return Status(grpc::NOT_FOUND, "");
      
        struct stat statBuf;
        stat(WrapPath(filename).c_str(), &statBuf);
        long fileLength = (long)statBuf.st_size;
        long readLength = 4000;
        long bytesTransferred = 0;
        char buffer[readLength];
        if (fileLength < readLength) {
            readLength = fileLength;
        }
        while (bytesTransferred < fileLength) {
            if (ctx->IsCancelled()) {
                return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline exceeded or Client cancelled, abandoning.");
            }
            if (fileLength - bytesTransferred < readLength)
                readLength = fileLength - bytesTransferred;
            file.seekg(bytesTransferred);
            file.read(buffer, readLength);
            response.set_filename(filename);
            response.set_contentsize(readLength);
            response.set_content(buffer, readLength);
            bytesTransferred += readLength;
            writer->Write(response);
        }
        file.close();
        return Status(grpc::OK, "");
    }

    Status destroyFile(ServerContext* ctx, const FileName* request, FileReply* response) override {
        string filename = request->filename();
        // struct stat statBuf;
        /*  int flag = stat(WrapPath(filename.c_str(), &statBuf));
        if (flag == -1) {
            return Status(grpc::NOT_FOUND, "");
        }*/
        
        if (ctx->IsCancelled()) {
            return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline exceeded or Client cancelled, abandoning.");
        }
        int del = remove(WrapPath(filename).c_str());
        if (del == -1) {
            return Status(grpc::NOT_FOUND, "");
        }
        else {
            return Status(grpc::OK, "");
        }
        
    }

    Status retrieveFiles(ServerContext* ctx, const listFiles* request, ListFilesResponse* response) override {
        //https://stackoverflow.com/questions/612097/how-can-i-get-the-list-of-files-in-a-directory-using-c-or-c
        DIR* dir;
        struct dirent* ent;
        map<string, int> mp;
        struct stat statBuf;

        if ((dir = opendir(WrapPath("").c_str())) != NULL) {
            while ((ent = readdir(dir)) != NULL) {
                if (ctx->IsCancelled()) {
                    return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline exceeded or Client cancelled, abandoning.");
                }
                stat(WrapPath(ent->d_name).c_str(), &statBuf);
                (*response->mutable_filelist())[ent->d_name] = statBuf.st_mtime;
            }
        }
        free(dir);
        free(ent);
        return Status(grpc::OK, "");
    }

    Status statusOfFile(ServerContext* ctx, const FileName* request, statusFile* response) override {
        string filename = request->filename();
        struct stat statBuf;
        int flag = stat(WrapPath(filename).c_str(), &statBuf);
        if (flag == -1) {
            return Status(grpc::NOT_FOUND, "");
        }
        else {
            response->set_filename(filename);
            response->set_creationtime(statBuf.st_mtime);
            response->set_modifiedtime(statBuf.st_mtime);
            response->set_filesize(statBuf.st_size);
            return Status(grpc::OK, "");
        }
    }
};

//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure. You may add additional methods or change these slightly,
// but be aware that the testing environment is expecting these three
// methods as-is.
//
/**
 * The main server node constructor
 *
 * @param server_address
 * @param mount_path
 */
DFSServerNode::DFSServerNode(const std::string &server_address,
        const std::string &mount_path,
        std::function<void()> callback) :
    server_address(server_address), mount_path(mount_path), grader_callback(callback) {}

/**
 * Server shutdown
 */
DFSServerNode::~DFSServerNode() noexcept {
    dfs_log(LL_SYSINFO) << "DFSServerNode shutting down";
    this->server->Shutdown();
}

/** Server start **/
void DFSServerNode::Start() {
    DFSServiceImpl service(this->mount_path);
    ServerBuilder builder;
    builder.AddListeningPort(this->server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    this->server = builder.BuildAndStart();
    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    this->server->Wait();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional DFSServerNode definitions here
//

