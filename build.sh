mkdir -p bin
cmake -DCMAKE_BUILD_TYPE=DEBUG -S . -B bin
cd 3rd-party/ilclient
source build.sh
cd ../..
cd bin
make -j4 VERBOSE=1
