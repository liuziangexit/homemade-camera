mkdir -p bin
cmake -DOPTIMIZATION=ON -DDEBUG=ON -S . -B bin
cd bin
make VERBOSE=1
