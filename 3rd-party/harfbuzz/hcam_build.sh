meson -Dprefix="$(pwd)/install" -Dfreetype=disabled build
cd build
ninja -j2
ninja install