git clone https://github.com/aws/aws-fpga.git $AWS_FPGA_REPO_DIR

cd $AWS_FPGA_REPO_DIR

source vitis_setup.sh

systemctl is-active --quiet mpd || sudo systemctl start mpd

cd ~

mkdir Server

cd Server

export PATH=/cvmfs/sft.cern.ch/lcg/contrib/CMake/3.7.0/Linux-x86_64/bin/:${PATH}

source /cvmfs/cms.cern.ch/cmsset_default.sh

git clone https://github.com/grpc/grpc

cd grpc

git checkout -b v1.27.0

git submodule update --init

mkdir -p cmake/build

cd cmake/build

cmake ../..

make -j 8

cd ../../examples/

sudo yum install autoconf automake libtool openssl openssl-devel

git clone https://github.com/protocolbuffers/protobuf.git

cd protobuf

git submodule update --init --recursive

./autogen.sh

./configure --prefix=/usr

make

make check

sudo make install

sudo ldconfig

cd ..

export PKG_CONFIG_PATH=${PWD}/../cmake/build/libs/opt/pkgconfig/

export PATH=${PWD}/../cmake/build/:${PATH}

export LD_LIBRARY_PATH=${PWD}/../grpc/cmake/build:${LD_LIBRARY_PATH}

git clone https://github.com/drankincms/grpc-trt-fgpa.git -b aws

cd grpc-trt-fgpa

git submodule update --init --recursive

make

./server hls4ml_c/aws_hls4ml.awsxclbin

