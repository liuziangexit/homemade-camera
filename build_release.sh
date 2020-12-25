mkdir -p bin
cmake -DCMAKE_BUILD_TYPE=RELEASE -S . -B bin
cd bin
make -j4 VERBOSE=1
