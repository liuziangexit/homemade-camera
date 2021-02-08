mkdir -p bin
BUILD_TYPE="DEBUG"
if [[ ! $# -eq 0 ]]; then
  BUILD_TYPE="$1"
fi

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
  cd 3rd-party/ilclient
  source build.sh
  cd ../..
fi

cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -S . -B bin

cd bin
make -j4 VERBOSE=1
