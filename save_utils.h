#include <string>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <fstream>
#include <linux/videodev2.h>
#include <sys/time.h>
extern "C" {
  #include <omap_drm.h>
  #include <omap_drmif.h>
  #include <xf86drmMode.h>
}

void printType(cv::Mat &mat);
void printInfo(cv::Mat &mat);
void save_data(void *img_data, int w, int h, const int out_channels, const int in_channels);
void write_binary_file(void *data, char *name, unsigned int size);
void print_v4l2buffer(v4l2_buffer *v);
void print_v4l2_plane_buffer(v4l2_buffer *v4l2buf);
void print_omap_bo(omap_bo *bo);
