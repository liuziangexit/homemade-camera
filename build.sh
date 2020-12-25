mkdir -p bin
cmake -DCMAKE_BUILD_TYPE=DEBUG -S . -B bin
cd bin
make -j4 VERBOSE=1
