mkdir -p bin
cmake -DCMAKE_BUILD_TYPE=DEBUG -S . -B bin
cd bin
make VERBOSE=1
