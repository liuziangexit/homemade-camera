ffmpeg -i ./demo.mov -c:v h264_omx -vf format=yuv420p -b:v 2048k demo_out_ff.mov
