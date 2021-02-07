#需要手动改下freetype2.pc
cmake -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX:PATH="$(pwd)/install" -S. -Bbuild
cd build
make install -j4
