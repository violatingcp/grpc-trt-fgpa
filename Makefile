#note the compile for below needs to be pointed in the right direction
LDFLAGS = -Wl,-rpath,/usr/lib \
 $(CURDIR)/../../cmake/build/libgrpc++_unsecure.a\
 $(CURDIR)/../../cmake/build/libgrpc.a\
 $(CURDIR)/../../cmake/build/libupb.a\
 /usr/lib/libprotobuf.a\
 -lpthread $(CURDIR)/../../cmake/build/libgrpc_unsecure.a\
 $(CURDIR)/../../cmake/build/libgpr.a\
 $(CURDIR)/../../cmake/build/third_party/cares/cares/lib/libcares.a\
 $(CURDIR)/../../cmake/build/libgrpc_plugin_support.a\
 $(CURDIR)/../../cmake/build/third_party/boringssl-with-bazel/libssl.a\
 $(CURDIR)/../../cmake/build/third_party/boringssl-with-bazel/libcrypto.a\
 $(CURDIR)/../../cmake/build/third_party/zlib/libz.a\
 $(CURDIR)/../../cmake/build/third_party/abseil-cpp/absl/strings/libabsl_strings.a\
 $(CURDIR)/../../cmake/build/third_party/abseil-cpp/absl/base/libabsl_base.a\
 $(CURDIR)/../../cmake/build/third_party/abseil-cpp/absl/base/libabsl_throw_delegate.a\
 $(CURDIR)/../../cmake/build/third_party/abseil-cpp/absl/base/libabsl_log_severity.a\
 $(CURDIR)/../../cmake/build/third_party/abseil-cpp/absl/base/libabsl_raw_logging_internal.a\
 $(CURDIR)/../../cmake/build/third_party/abseil-cpp/absl/hash/libabsl_hash.a\
 $(CURDIR)/../../cmake/build/third_party/abseil-cpp/absl/time/libabsl_time.a\
 $(CURDIR)/../../cmake/build/third_party/abseil-cpp/absl/time/libabsl_civil_time.a\
 $(CURDIR)/../../cmake/build/third_party/abseil-cpp/absl/time/libabsl_time_zone.a\
 $(CURDIR)/../../cmake/build/third_party/abseil-cpp/absl/strings/libabsl_str_format_internal.a\
 $(CURDIR)/../../cmake/build/third_party/abseil-cpp/absl/strings/libabsl_strings_internal.a\
 $(CURDIR)/../../cmake/build/third_party/abseil-cpp/absl/types/libabsl_bad_optional_access.a\
 $(CURDIR)/../../cmake/build/third_party/abseil-cpp/absl/types/libabsl_bad_any_cast_impl.a\
 $(CURDIR)/../../cmake/build/third_party/abseil-cpp/absl/types/libabsl_bad_variant_access.a\
 $(CURDIR)/../../cmake/build/third_party/abseil-cpp/absl/numeric/libabsl_int128.a\
 -lnsl $(CURDIR)/../../cmake/build/libaddress_sorting.a\
 $(opencl_LDFLAGS)\
 -ldl -lrt -lm -lssl
#/usr/local/lib/libgrpc++_unsecure.a /usr/local/lib64/libprotobuf.a -lpthread /usr/local/lib/libgrpc_unsecure.a /usr/local/lib/libgpr.a /usr/local/lib/libz.so /usr/local/lib/libcares.so.2.3.0 -lnsl /usr/local/lib/libaddress_sorting.a -ldl -lrt -lm 

COMMON_REPO := ./hls4ml_c/
#include $(COMMON_REPO)/utility/utils.mk
include $(COMMON_REPO)/libs/xcl2/xcl2.mk
include $(COMMON_REPO)/libs/opencl/opencl.mk

HLS4ML_PROJ_TYPE := DENSE

CXX = g++
CPPFLAGS += `pkg-config --cflags protobuf grpc` -I$(CURDIR)/../../include -I$(COMMON_REPO)/src/ -I$(COMMON_REPO)/src/nnet_utils/ $(xcl2_CXXFLAGS) $(opencl_CXXFLAGS) -DIS_$(HLS4ML_PROJ_TYPE) -DWEIGHTS_DIR=$(COMMON_REPO)/src/weights/ -I$(XILINX_VIVADO)/include/ -I$(XILINX_VITIS)/include/ -Wno-unknown-pragmas -I$(XILINX_XRT)/include/
CXXFLAGS += -std=c++11

GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

all: server client 

xcl2.o: $(xcl2_SRCS) $(xcl2_HDRS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

client: api.pb.o model_config.pb.o request_status.pb.o server_status.pb.o grpc_service.pb.o api.grpc.pb.o model_config.grpc.pb.o request_status.grpc.pb.o server_status.grpc.pb.o grpc_service.grpc.pb.o client.o
	$(CXX) $^ $(LDFLAGS) -o $@

server: api.pb.o model_config.pb.o request_status.pb.o server_status.pb.o grpc_service.pb.o api.grpc.pb.o model_config.grpc.pb.o request_status.grpc.pb.o server_status.grpc.pb.o grpc_service.grpc.pb.o server.o xcl2.o
	$(CXX) $^ $(LDFLAGS) -o $@

server_nofpga: api.pb.o model_config.pb.o request_status.pb.o server_status.pb.o grpc_service.pb.o api.grpc.pb.o model_config.grpc.pb.o request_status.grpc.pb.o server_status.grpc.pb.o grpc_service.grpc.pb.o server_nofgpa.o xcl2.o
	$(CXX) $^ $(LDFLAGS) -o $@

%.grpc.pb.cc: %.proto
	protoc --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

%.pb.cc: %.proto
	protoc --cpp_out=. $<

clean:
	rm -f *.o *.pb.cc *.pb.h server server_nofpga
