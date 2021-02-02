rm -f nohup.out

BUILD_TYPE="DEBUG"
if [[ ! $# -eq 0 ]]; then
  BUILD_TYPE="$1"
fi

nohup ./build.sh "$BUILD_TYPE" &
