mkdir -p bin
BUILD_TYPE="DEBUG"
if [[ ! $# -eq 0 ]]; then
  BUILD_TYPE="$1"
fi

cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -S . -B bin
cd 3rd-party/ilclient
source build.sh
cd ../..
cd bin
make -j4 VERBOSE=1
