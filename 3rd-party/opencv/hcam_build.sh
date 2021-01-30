export LD_LIBRARY_PATH="$LD_LIBRARY_PATH ../ffmpeg/ffinstall/lib/ /opt/vc/lib/"
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:../ffmpeg/ffinstall/lib/pkgconfig
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/opt/vc/lib/pkgconfig
export PKG_CONFIG_LIBDIR=$PKG_CONFIG_LIBDIR:../ffmpeg/ffinstall/lib/
export PKG_CONFIG_LIBDIR=$PKG_CONFIG_LIBDIR:/opt/vc/lib/
cmake --enable-nonfree --enable-pic --enable-shared \
  -DWITH_FFMPEG=ON -DWITH_AVFOUNDATION=OFF \
  -DBUILD_TESTS=OFF -DBUILD_PREF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_opencv_apps=OFF \
  -DWITH_CUDA=OFF \
  -DBUILD_opencv_calib3d=OFF -DBUILD_opencv_dnn=OFF -DBUILD_opencv_features2d=OFF \
  -DBUILD_opencv_flann=OFF -DBUILD_opencv_gapi=OFF -DBUILD_opencv_ml=OFF -DBUILD_opencv_objdetect=OFF -DBUILD_opencv_stitching=OFF -DBUILD_opencv_ts=OFF \
  -DBUILD_OPENCV_PYTHON3=OFF -DBUILD_OPENCV_PYTHON2=OFF \
  -DOPENCV_EXTRA_MODULES_PATH=../opencv_contrib/modules/ \
  -D CMAKE_BUILD_TYPE=RELEASE -D ENABLE_NEON=ON -D ENABLE_VFPV3=ON \
  -S. -Bbuild
cd build
make -j4
