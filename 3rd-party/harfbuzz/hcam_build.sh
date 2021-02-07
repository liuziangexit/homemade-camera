export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:../freetype/install/lib/pkgconfig"
meson -Dprefix="$(pwd)/install" -Dfreetype=enabled build
cd build
ninja -j4
ninja install
