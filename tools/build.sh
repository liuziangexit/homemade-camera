#source ./clean.sh
cd ..
mkdir -p bin
cmake -DOPTIMIZATION=OFF -DDEBUG=ON -S . -B bin
cd bin
make VERBOSE=1
cd ../tools
