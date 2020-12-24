mkdir -p bin
cmake -DCMAKE_BUILD_TYPE=RELEASE -S . -B bin
cd bin
make VERBOSE=1
