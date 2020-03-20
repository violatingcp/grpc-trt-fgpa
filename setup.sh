#!/bin/bash

cd /home/centos/src/project_data/aws-fpga
source /opt/Xilinx/Vitis/2019.2/settings64.sh 
source /opt/xilinx/xrt/setup.sh 
source vitis_runtime_setup.sh 
source sdk_setup.sh 
cd /home/centos/Server/grpc/examples/grpc-trt-fgpa
