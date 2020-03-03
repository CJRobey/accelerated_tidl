/* This file defines the function to program V4L2 device */

#include <unistd.h>
#include <errno.h>
#include <string>
#include <string.h>
#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include "v4l2_obj.h"
#include "error.h"

#define CAP_WIDTH 800
#define CAP_HEIGHT 600

#define TIDL_MODEL_WIDTH 768
#define TIDL_MODEL_HEIGHT 320

/*
* Initialize the app resources with default parameters
*/
void VIPObj::default_parameters(void) {
    /* Main camera */
    m_dev_name = "/dev/video1";

    src.num_buffers = NBUF;
    src.fourcc = V4L2_PIX_FMT_YUYV;
    src.colorspace = V4L2_COLORSPACE_SMPTE170M;
    src.width = CAP_WIDTH;
    src.height = CAP_HEIGHT;
    src.memory = V4L2_MEMORY_DMABUF;
    src.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    src.coplanar = false;
    src.v4l2bufs = NULL;
    return;
}


void VIPObj::device_init(){
    struct v4l2_capability capability;
    struct v4l2_streamparm streamparam;
    int ret;

    /* Open the capture device */
    m_fd = open(m_dev_name.c_str(), O_RDWR);

    if (m_fd <= 0) {
        ERROR("Cannot open %s device\n\n", m_dev_name.c_str());
        return;
    }

    MSG("\n%s: Opened Channel at fd %d\n", m_dev_name.c_str(), m_fd);

    /* Check if the device is capable of streaming */
    if (ioctl(m_fd, VIDIOC_QUERYCAP, &capability) < 0) {
        perror("VIDIOC_QUERYCAP");
        goto ERR;
    }

    if (capability.capabilities & V4L2_CAP_STREAMING)
        MSG("%s: Capable of streaming\n", m_dev_name.c_str());
    else {
        ERROR("%s: Not capable of streaming\n", m_dev_name.c_str());
        goto ERR;
    }

    streamparam.type = src.type;
    if (ioctl(m_fd, VIDIOC_G_PARM, &streamparam) < 0){
        ERROR("VIDIOC_G_PARM");
        goto ERR;
    }

    src.fmt.type = src.type;
    ret = ioctl(m_fd, VIDIOC_G_FMT, &src.fmt);
    if (ret < 0) {
        ERROR("VIDIOC_G_FMT failed: %s (%d)", strerror(errno), ret);
        goto ERR;
    }

    src.fmt.type = src.type;
    src.fmt.fmt.pix.width = src.width;
    src.fmt.fmt.pix.height = src.height;
    src.fmt.fmt.pix.pixelformat = src.fourcc;
    //src.fmt.fmt.pix.field = V4L2_FIELD_ALTERNATE;
    src.fmt.fmt.pix.colorspace = src.colorspace;

    ret = ioctl(m_fd, VIDIOC_S_FMT, &src.fmt);
    if (ret < 0) {
        ERROR("VIDIOC_S_FMT");
        goto ERR;
    }

    MSG("%s: Init done successfully\n", m_dev_name.c_str());
    MSG("%s: VIP: G_FMT(start): width = %u, height = %u, 4cc = %.4s\n",
        m_dev_name.c_str(), src.fmt.fmt.pix.width, src.fmt.fmt.pix.height,
        (char*)&src.fmt.fmt.pix.pixelformat);
    return;
ERR:
    close(m_fd);
    return;
}


VIPObj::VIPObj(){
  default_parameters();
}

VIPObj::VIPObj(std::string dev_name, int w, int h, int pix_fmt, int num_buf,
  int type, int memory){
    default_parameters();
    m_dev_name = dev_name;
    src.width = w;
    src.height = h;
    src.num_buffers = num_buf;
    src.type=(v4l2_buf_type) type;
    src.fourcc = pix_fmt;
    if (src.fourcc == V4L2_PIX_FMT_YUYV)
      src.size = w*h*2;
    src.memory = memory;
    if (src.memory == V4L2_MEMORY_MMAP)
      src.fmt.fmt.pix.field = V4L2_FIELD_NONE;
    else
      src.fmt.fmt.pix.field = V4L2_FIELD_ALTERNATE;
}

VIPObj::~VIPObj(){
    free(src.v4l2bufs);
    close(m_fd);
    return;
}



/* In this example application, user space allocates the buffers only
*/
bool VIPObj::request_buf(){
    struct v4l2_requestbuffers reqbuf;
    int ret;
    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type = src.type;
    reqbuf.memory = src.memory;
    reqbuf.count = src.num_buffers;

    ret = ioctl(m_fd, VIDIOC_REQBUFS, &reqbuf);
    if (ret < 0) {
        ERROR("VIDIOC_REQBUFS failed: %s (%d)", strerror(errno), ret);
        return false;
    }

    src.num_buffers = reqbuf.count;
    MSG("Allocated %d buffers", reqbuf.count);

    return true;
}


// /* In this example application, user space allocates the buffers and
//  * provides the buffer fd to be exported to the V4L2 driver
// */
// bool VIPObj::request_export_buf(int * export_fds){
//     struct v4l2_requestbuffers reqbuf;
//     int ret;
//     memset(&reqbuf, 0, sizeof(reqbuf));
//     reqbuf.type = src.type;
//     reqbuf.memory = src.memory;
//     reqbuf.count = src.num_buffers;
//
//     ret = ioctl(m_fd, VIDIOC_REQBUFS, &reqbuf);
//     if (ret < 0) {
//         ERROR("VIDIOC_REQBUFS failed: %s (%d)", strerror(errno), ret);
//         return false;
//     }
//
//     src.num_buffers = reqbuf.count;
//     MSG("Allocated %d buffers", reqbuf.count);
//
//     src.v4l2bufs = (struct v4l2_buffer *) calloc( reqbuf.count, sizeof(struct v4l2_buffer));
//
//     for (int i=0;i<(int)reqbuf.count; i++) {
//       memset(src.v4l2bufs[i], 0, sizeof(*src.v4l2bufs[i]));
//       src.v4l2bufs[i]->type = src.type;
//       src.v4l2bufs[i]->memory = src.memory;
//       src.v4l2bufs[i]->index = i;
//
//       ret = ioctl(m_fd, VIDIOC_QUERYBUF, src.v4l2bufs[i]);
//       if (ret) {
//   			ERROR("VIDIOC_QUERYBUF failed: %s (%d)", strerror(errno), ret);
//   			return false;
//       }
//
//       src.v4l2bufs[i]->m.fd = export_fds[i];
//       MSG("Query buf #%d - exporting fd %d", i, export_fds[i]);
//       print_v4l2buffer(src.v4l2bufs[i]);
//     }
//
//     return true;
// }

/* In this example application, user space allocates the buffers and
 * provides the buffer fd to be exported to the V4L2 driver
*/
bool VIPObj::request_export_buf(int * export_fds){
    struct v4l2_requestbuffers reqbuf;
    int ret;
    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type = src.type;
    reqbuf.memory = src.memory;
    reqbuf.count = src.num_buffers;

    ret = ioctl(m_fd, VIDIOC_REQBUFS, &reqbuf);
    if (ret < 0) {
        ERROR("VIDIOC_REQBUFS failed: %s (%d)", strerror(errno), ret);
        return false;
    }

    src.num_buffers = reqbuf.count;
    MSG("Allocated %d buffers", reqbuf.count);

    src.v4l2bufs = (struct v4l2_buffer **) malloc(reqbuf.count * sizeof(unsigned int));
    if (src.memory == V4L2_MEMORY_MMAP)
      src.base_addr = (unsigned int **) calloc(src.num_buffers, sizeof(unsigned int));

    for (int i=0;i<(int)reqbuf.count; i++) {
      src.v4l2bufs[i] = (struct v4l2_buffer *) malloc(sizeof(struct v4l2_buffer));
      //memset(src.v4l2bufs[i], 0, sizeof(struct v4l2_buffer));
      src.v4l2bufs[i]->type = src.type;
      src.v4l2bufs[i]->memory = src.memory;
      src.v4l2bufs[i]->index = i;

      ret = ioctl(m_fd, VIDIOC_QUERYBUF, src.v4l2bufs[i]);

      if (ret) {
  			ERROR("VIDIOC_QUERYBUF failed: %s (%d)", strerror(errno), ret);
  			return false;
      }

      // check the memory type as to whether you should populate mmap or dmabuf
      if (src.memory == V4L2_MEMORY_DMABUF) {
        if (!export_fds) {
          ERROR("NULL export file descriptors with DMABUF memory type");
          return false;
        }
        src.v4l2bufs[i]->m.fd = export_fds[i];
        MSG("Query buf #%d - exporting fd %d", i, export_fds[i]);
      }
      else if (src.memory == V4L2_MEMORY_MMAP) {
        src.base_addr[i] = (unsigned int *) mmap(NULL, src.v4l2bufs[i]->length, PROT_READ | PROT_WRITE,
               MAP_SHARED, m_fd, src.v4l2bufs[i]->m.offset);
        if (!src.base_addr[i]) {
          ERROR("mmap failed!");
          sleep(2);
        }
        MSG("Length: %d\nAddress: %p", src.v4l2bufs[i]->length, src.base_addr[i]);
      }

      // print_v4l2buffer(src.v4l2bufs[i]);

    }
    return true;
}




/*
* Queue V4L2 buffer
*/
bool VIPObj::queue_export_buf(int fd, int index){
    struct v4l2_buffer buf;
  	int			ret = -1;

    memset(&buf, 0, sizeof buf);
  	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  	buf.memory = V4L2_MEMORY_DMABUF;
  	buf.index = index;
  	buf.m.fd = fd;

    ret = ioctl(m_fd, VIDIOC_QBUF, &buf);
    if (ret) {
        ERROR("VIDIOC_QBUF failed: %s (%d)", strerror(errno), ret);
        return false;
    }

    return true;
}

/*
* Queue V4L2 buffer
*/
bool VIPObj::queue_buf(int fd, int index) {
    struct v4l2_buffer *buf = NULL;
  	int			ret = -1;

    if (src.memory == V4L2_MEMORY_MMAP) {
      MSG("Queueing MMAP buffer");
      for (int i=0; i<src.num_buffers; i++)
        if (src.v4l2bufs[i]->index == (unsigned int) index)
          buf = src.v4l2bufs[i];
    }
    else {
      for (int i=0; i<src.num_buffers; i++)
        if (src.v4l2bufs[i]->m.fd == fd)
          buf = src.v4l2bufs[i];
    }
    // print_v4l2buffer(buf);
    ret = ioctl(m_fd, VIDIOC_QBUF, buf);

    if (ret) {
        ERROR("VIDIOC_QBUF failed: %s (%d)", strerror(errno), ret);
        return false;
    }

    return true;
}

/*
* DeQueue V4L2 buffer
*/
int VIPObj::dequeue_buf(VPEObj *vpe){
    struct v4l2_buffer v4l2buf;
    int ret;

    memset(&v4l2buf, 0, sizeof(v4l2buf));

    v4l2buf.type = src.type;
    v4l2buf.memory = src.memory;
    ret = ioctl(m_fd, VIDIOC_DQBUF, &v4l2buf);
    // print_v4l2buffer(&v4l2buf);
    MSG("m_fd is %d", m_fd);
    if (ret) {
        ERROR("VIDIOC_DQBUF failed: %s (%d)\n", strerror(errno), ret);
        return -1;
    }
    if (vpe)
      vpe->m_field = v4l2buf.field;

    return v4l2buf.index;
}


/*
* DeQueue V4L2 buffer
*/
int VIPObj::dequeue_buf() {
    struct v4l2_buffer v4l2buf;
    memset(&v4l2buf, 0, sizeof v4l2buf);

    v4l2buf.type = src.type;
  	v4l2buf.memory = src.memory;
    int ret = ioctl(m_fd, VIDIOC_DQBUF, &v4l2buf);
    if (ret) {
        ERROR("VIDIOC_DQBUF failed: %s (%d)\n", strerror(errno), ret);
        return -1;
    }
    return v4l2buf.index;
}

/*
* Enable streaming for V4L2 capture device
*/
bool VIPObj::stream_on(){
    enum v4l2_buf_type type = (v4l2_buf_type) src.type;
    int ret = 0;

    ret = ioctl(m_fd, VIDIOC_STREAMON, &type);

    if (ret) {
        ERROR("VIDIOC_STREAMON failed: %s (%d)", strerror(errno), ret);
        return false;
    }

    return true;
}

/*
* Disable streaming for V4L2 capture device
*/
int VIPObj::stream_off(){
    enum v4l2_buf_type type = (v4l2_buf_type) src.type;
    int ret = -1;

    if (m_fd <= 0) {
        return ret;
    }

    ret = ioctl(m_fd, VIDIOC_STREAMOFF, &type);

    if (ret) {
        ERROR("VIDIOC_STREAMOFF failed: %s (%d)", strerror(errno), ret);
    }

    return ret;
}
