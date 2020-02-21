#include <string>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <fstream>

void printType(cv::Mat &mat);
void printInfo(cv::Mat &mat);
void save_data(void *img_data, int w, int h, const int out_channels, const int in_channels);
void write_binary_file(void *data, char *name, unsigned int size);
