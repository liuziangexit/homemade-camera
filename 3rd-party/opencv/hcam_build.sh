export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:../ffmpeg/ffinstall/lib/pkgconfig:../vc/pkgconfig:../freetype/install/lib/pkgconfig:../harfbuzz/install/lib/arm-linux-gnueabihf/pkgconfig"

cmake --enable-nonfree --enable-pic --enable-shared \
  -DBUILD_LIST=core,ffmpeg,freetype,video,videoio,photo,imgcodecs,imgproc \
  -DBUILD_TESTS=OFF -DBUILD_PREF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_opencv_apps=OFF \
  -DOPENCV_EXTRA_MODULES_PATH=../opencv_contrib/modules/ \
  -D CMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX:PATH="$(pwd)/install" -D ENABLE_NEON=ON -D ENABLE_VFPV3=ON \
  -S. -Bbuild
cd build
make install -j4
