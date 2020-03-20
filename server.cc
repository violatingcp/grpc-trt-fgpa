#include <iostream>
#include <memory>
#include <string>

#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <chrono>
typedef std::chrono::high_resolution_clock Clock;

#include "xcl2.hpp"
#include <vector>

#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"
#include "request_status.grpc.pb.h"
#include "model_config.pb.h"

#include "kernel_params.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using nvidia::inferenceserver::GRPCService;
using nvidia::inferenceserver::InferRequest;
using nvidia::inferenceserver::InferResponse;
using nvidia::inferenceserver::StatusRequest;
using nvidia::inferenceserver::StatusResponse;
using nvidia::inferenceserver::RequestStatusCode;
using nvidia::inferenceserver::DataType;

class GRPCServiceImplementation final : public nvidia::inferenceserver::GRPCService::Service {

 public:
  std::vector<bigdata_t,aligned_allocator<bigdata_t>> source_in;
  std::vector<bigdata_t,aligned_allocator<bigdata_t>> source_hw_results;
//(DATA_SIZE_IN*STREAMSIZE);
//(DATA_SIZE_OUT*STREAMSIZE);
  std::vector<cl::Memory> inBufVec, outBufVec;
  cl::Kernel krnl_xil;
  cl::Program program;
  cl::CommandQueue q;

 private: 
  std::vector<cl::Event>   waitInput_;//m_Mem2FpgaEvents;                                                                                                                                                                                    
  std::vector<cl::Event>   waitOutput_;//m_ExeKernelEvents;                                                                                                                                                                                  

  grpc::Status Status(
		     ServerContext* context, 
		     const StatusRequest* request, 
		     StatusResponse* reply
		     ) override {
    auto server_status = reply->mutable_server_status();
    server_status->set_id("inference:0");
    auto& model_status  = *server_status->mutable_model_status();
    auto config = model_status["facile"].mutable_config();
    config->set_max_batch_size(160000);
    //Hcal config
    config->set_name("facile");
    auto input = config->add_input();
    input->set_name("input");
    input->set_data_type(DataType::TYPE_UINT16);
    input->add_dims(32);
    auto output = config->add_output();
    output->set_name("output/BiasAdd");
    output->set_data_type(DataType::TYPE_UINT16);
    output->add_dims(1);
    reply->mutable_request_status()->set_code(RequestStatusCode::SUCCESS);
    nvidia::inferenceserver::RequestStatus request_status = reply->request_status();
    nvidia::inferenceserver::ServerStatus  check_server_status  = reply->server_status();
    return grpc::Status::OK;
  }

  grpc::Status Infer(
		     ServerContext* context, 
		     const InferRequest* request, 
		     InferResponse* reply
		     ) override {
    std::cout << " infer " << std::endl;
    auto t0 = Clock::now();
    const std::string& raw = request->raw_input(0);
    const void* lVals = raw.c_str();
    data_t* lFVals = (data_t*) lVals;
    //output array that is equal to ninputs(15)*batch flot is 4 bits
    unsigned batch_size = raw.size()/32/sizeof(data_t);
    reply->mutable_request_status()->set_code(RequestStatusCode::SUCCESS);
    reply->mutable_request_status()->set_server_id("inference:0");
    reply->mutable_meta_data()->set_id(request->meta_data().id());
    reply->mutable_meta_data()->set_model_version(-1);
    reply->mutable_meta_data()->set_batch_size(batch_size);

    //setup output (this is critical)
    auto output1 = reply->mutable_meta_data()->add_output();
    output1->set_name("output/BiasAdd");
    output1->mutable_raw()->mutable_dims()->Add(1);
    output1->mutable_raw()->set_batch_byte_size(2*sizeof(data_t)*batch_size);

    memcpy(source_in.data(), &lFVals[0], batch_size*sizeof(bigdata_t));
    auto t1a = Clock::now();
    cl::Event l_event_in;                                                                                                                                                                                                                  
    q.enqueueMigrateMemObjects(inBufVec,0/* 0 means from host*/,NULL,&l_event_in);                                                                                                                                                         
    //q.enqueueMigrateMemObjects(inBufVec,0/* 0 means from host*/);
    auto t1b = Clock::now();
    cl::Event l_event;                                                                                                                                                                                                                  
    q.enqueueTask(krnl_xil,&(waitInput_),&l_event);  
    //q.enqueueTask(krnl_xil);
    auto t1c = Clock::now();
    cl::Event l_event_out;                                                                                                                                                                                                                 
    q.enqueueMigrateMemObjects(outBufVec,CL_MIGRATE_MEM_OBJECT_HOST,&waitOutput_,&l_event_out);  
    //q.enqueueMigrateMemObjects(outBufVec,CL_MIGRATE_MEM_OBJECT_HOST);
    l_event_out.wait();
    auto t1 = Clock::now();
    q.finish();
    //clFinish(q.get());
    auto t2 = Clock::now();
    //std::cout << "FPGA time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count() << " ns" << batch_size << std::endl;

    //std::cout<<source_hw_results[0]<<std::endl;
    std::string *outputs1 = reply->add_raw_output();
    char* lTVals = new char[batch_size*sizeof(data_t)];//2*batch_size*sizeof(data_t)];
    memcpy(&lTVals[0], source_hw_results.data(), batch_size*sizeof(data_t));
    outputs1->append(lTVals,(batch_size)*sizeof(data_t));
    auto t3 = Clock::now();
    std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t0).count() << " ns" << std::endl;
    std::cout << "   T1 time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() << " ns" << std::endl;
    std::cout << "   T2 time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t2).count() << " ns" << std::endl;
    std::cout << " FPGA time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count() << " ns" << std::endl;
    std::cout << " FPGA time: +t1 " << std::chrono::duration_cast<std::chrono::nanoseconds>(t1a - t1).count() << " ns" << std::endl;
    std::cout << " FPGA time: +inf" << std::chrono::duration_cast<std::chrono::nanoseconds>(t1b - t1).count() << " ns" << std::endl;
    std::cout << " FPGA time: +t2" << std::chrono::duration_cast<std::chrono::nanoseconds>(t1c - t1).count() << " ns" << std::endl;
    return grpc::Status::OK;
  } 
};

void Run(std::string xclbinFilename) {
  std::string address("0.0.0.0:8443");
  GRPCServiceImplementation service;

  size_t vector_size_in_bytes  = sizeof(bigdata_t) * STREAMSIZE;
  size_t vector_size_out_bytes = sizeof(bigdata_t) * COMPSTREAMSIZE;
  service.source_in.reserve(STREAMSIZE);
  service.source_hw_results.reserve(COMPSTREAMSIZE);

  std::vector<cl::Device> devices = xcl::get_xil_devices();
  cl::Device device = devices[0];
  devices.resize(1);

  cl::Context context(device);
  cl::CommandQueue q_tmp(context, device, CL_QUEUE_PROFILING_ENABLE);
  service.q = q_tmp;
  std::string device_name = device.getInfo<CL_DEVICE_NAME>();
  std::cout << "Found Device=" << device_name.c_str() << std::endl;

  cl::Program::Binaries bins;
  // Load xclbin
  std::cout << "Loading: '" << xclbinFilename << "'\n";
  std::ifstream bin_file(xclbinFilename, std::ifstream::binary);
  bin_file.seekg (0, bin_file.end);
  unsigned nb = bin_file.tellg();
  bin_file.seekg (0, bin_file.beg);
  char *buf = new char [nb];
  bin_file.read(buf, nb);
  // Creating Program from Binary File
  bins.push_back({buf,nb});
  cl::Program tmp_program(context, devices, bins);
  service.program = tmp_program;
  cl::Kernel krnl_aws_hls4ml(service.program,"aws_hls4ml");
  service.krnl_xil = krnl_aws_hls4ml;
  // Allocate Buffer in Global Memory
  // Buffers are allocated using CL_MEM_USE_HOST_PTR for efficient memory and 
  // Device-to-host communication
  cl::Buffer buffer_in    (context,CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,   vector_size_in_bytes, service.source_in.data());
  cl::Buffer buffer_output(context,CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, vector_size_out_bytes, service.source_hw_results.data());
  service.inBufVec.clear();
  service.outBufVec.clear();
  service.inBufVec.push_back(buffer_in);
  service.outBufVec.push_back(buffer_output);


  int narg = 0;
  service.krnl_xil.setArg(narg++, buffer_in);
  service.krnl_xil.setArg(narg++, buffer_output);

  ServerBuilder builder;
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.SetMaxMessageSize(1000000);
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on port: " << address << std::endl;
  int server_id=1;
  server->Wait();
}

int main(int argc, char** argv) {

  std::string xclbinFilename = "";
  if (argc>1) xclbinFilename = argv[1];
  Run(xclbinFilename);

  return 0;
}
