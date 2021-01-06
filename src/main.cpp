#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "linux/videodev2.h"
void printFrameInterval(int fd, unsigned int fmt, unsigned int width, unsigned int height)
{
  struct v4l2_frmivalenum frmival;
  memset(&frmival,0,sizeof(frmival));
  frmival.pixel_format = fmt;
  frmival.width = width;
  frmival.height = height;
  while (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0)
  {
    if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE)
      printf("[%dx%d] %f fps\n", width, height, 1.0*frmival.discrete.denominator/frmival.discrete.numerator);
    else
      printf("[%dx%d] [%f,%f] fps\n", width, height, 1.0*frmival.stepwise.max.denominator/frmival.stepwise.max.numerator, 1.0*frmival.stepwise.min.denominator/frmival.stepwise.min.numerator);
    frmival.index++;
  }
}

int main(int argc, char **argv) {
  unsigned int width=0, height=0;;
  int fd = open("/dev/video0", O_RDWR);
  struct v4l2_frmsizeenum frmsize;
  memset(&frmsize,0,sizeof(frmsize));
  frmsize.pixel_format = V4L2_PIX_FMT_JPEG;
  while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0)
  {
    if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
    {
      printFrameInterval(fd, frmsize.pixel_format, frmsize.discrete.width, frmsize.discrete.height);
    }
    else
    {
      for (width=frmsize.stepwise.min_width; width< frmsize.stepwise.max_width; width+=frmsize.stepwise.step_width)
        for (height=frmsize.stepwise.min_height; height< frmsize.stepwise.max_height; height+=frmsize.stepwise.step_height)
          printFrameInterval(fd, frmsize.pixel_format, width, height);

    }
    frmsize.index++;
  }
  return 0;
}