cmake -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX:PATH="$(pwd)/install" -DTBB4PY_BUILD=OFF -DTBB_TEST=OFF -S. -Bbuild
cd build
make install -j4
