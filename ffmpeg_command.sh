ffmpeg -f v4l2 -input_format mjpeg \
 -framerate 30 -video_size 1280x720 -i /dev/video0 \
 -c:v h264_omx -vf format=yuv420p -b:v 2048k output.mov
