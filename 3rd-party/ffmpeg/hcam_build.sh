./configure --prefix="$(pwd)/ffinstall" --extra-ldflags=" -latomic " --enable-gpl --enable-version3 --enable-nonfree --disable-ffplay \
  --disable-network --enable-avresample \
  --enable-encoder=libx264 --enable-decoder=h264 --enable-encoder=aac --enable-decoder=aac --enable-encoder=ac3 --enable-decoder=ac3 \
  --enable-encoder=rawvideo --enable-decoder=rawvideo --enable-encoder=mjpeg --enable-decoder=mjpeg --enable-demuxer=concat --enable-muxer=flv --enable-demuxer=flv --enable-demuxer=live_flv --enable-muxer=hls --enable-muxer=segment --enable-muxer=stream_segment --enable-muxer=mov --enable-demuxer=mov --enable-muxer=mp4 --enable-muxer=mpegts --enable-demuxer=mpegts --enable-demuxer=mpegvideo --enable-muxer=pcm* --enable-demuxer=pcm* --enable-muxer=rawvideo --enable-demuxer=rawvideo --enable-muxer=fifo --enable-muxer=tee --enable-parser=h264 --enable-parser=aac \
  --disable-libx264 \
  --enable-protocol=file --enable-protocol=cache --enable-protocol=pipe \
  --enable-encoder=h264_v4l2m2m --enable-encoder=h264_vaapi --enable-omx --enable-omx-rpi --enable-encoder=h264_omx --enable-mmal --enable-hwaccel=h264_mmal \
  --enable-static --disable-shared \
  --disable-doc --disable-htmlpages --disable-podpages --disable-txtpages --disable-manpages

make install -j4
#--enable-debug --disable-optimizations --disable-asm
