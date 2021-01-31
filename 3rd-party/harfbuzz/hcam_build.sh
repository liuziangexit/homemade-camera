meson -Dprefix="$(pwd)/install" build
cd build
ninja -j2
ninja install