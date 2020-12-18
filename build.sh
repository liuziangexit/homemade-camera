#source ./clean.sh
mkdir -p bin
cmake -DOPTIMIZATION=OFF -DDEBUG=ON -S . -B bin
cd bin
make VERBOSE=1
