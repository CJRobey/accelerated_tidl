#include <linux/videodev2.h>
#include <string>
#include "cmem_buf.h"

#define NBUF (3)

#define CAP_WIDTH 800
#define CAP_HEIGHT 600

#define MODEL_WIDTH 768
#define MODEL_HEIGHT 332

class ImageParams {
public:
  unsigned int **base_addr;
  int width;
  int height;
  int fourcc;
  int size;
  int type;
  int size_uv;
  bool coplanar;
  int memory;
  v4l2_format fmt;
  v4l2_colorspace colorspace;
  v4l2_buffer *v4l2buf;
  int num_buffers;
};


class VPEObj {
public:
  int m_fd;
  int m_deinterlace;
  int m_field;
  int m_num_buffers;
  ImageParams src;
  ImageParams dst;

  VPEObj();
  VPEObj(int src_w, int src_h, int src_bytes_per_pixel, int src_fourcc,
    int dst_w, int dst_h, int dst_bytes_per_pixel, int dst_fourcc, int num_buffers);
  ~VPEObj();
  bool open_fd(void);
  void vpe_close();
  int set_src_format();
  int set_dst_format();
  bool vpe_input_init(int *fd);
  bool vpe_output_init();
  bool input_qbuf(int fd, int index);
  bool output_qbuf(int index);
  bool stream_on(int layer);
  int stream_off();
  int input_dqbuf();
  int output_dqbuf();
  int display_buffer(int index);

private:
  bool set_ctrl();
  void default_parameters();
  std::string m_dev_name;
  int m_translen;
};


class VIPObj {
public:
  int m_fd;
  ImageParams src;

  VIPObj();
  VIPObj(std::string dev_name, int w, int h, int pix_fmt, int num_buf, int type);
  ~VIPObj();
  int set_format();
  void device_init();
  bool queue_buf(int fd, int index);
  bool request_buf();
  bool stream_on();
  int stream_off();
  int dequeue_buf(VPEObj *vpe);
  int display_buffer(int index);

private:
  void default_parameters();
  std::string m_dev_name;
};
