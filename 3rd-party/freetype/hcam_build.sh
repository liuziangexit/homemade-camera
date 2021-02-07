export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:../harfbuzz/install/lib/pkgconfig"
cmake -DFT_WITH_HARFBUZZ=true -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX:PATH="$(pwd)/install" -S. -Bbuild
cd build
make install -j4