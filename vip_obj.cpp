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

#define NBUF (3)
#define CAP_WIDTH 800
#define CAP_HEIGHT 600

#define MODEL_WIDTH 768
#define MODEL_HEIGHT 332

/*
* Initialize the app resources with default parameters
*/
void VIPObj::default_parameters(void) {
    /* Main camera */
    m_dev_name = "/dev/video1";

    src.num_buffers = NBUF;
    src.fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    src.width = CAP_WIDTH;
    src.height = CAP_HEIGHT;
    src.memory = V4L2_MEMORY_MMAP;
    src.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    src.coplanar = false;
    src.v4l2buf = NULL;
    return;
}


void VIPObj::device_init(int pix_fmt){
    struct v4l2_capability capability;
    struct v4l2_streamparm streamparam;
    int ret;

    /* Open the capture device */
    m_fd = open(m_dev_name.c_str(), O_RDWR);

    if (m_fd <= 0) {
        ERROR("Cannot open %s device\n\n", m_dev_name.c_str());
        return;
    }

    MSG("\n%s: Opened Channel\n", m_dev_name.c_str());

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
    src.fmt.fmt.pix.pixelformat = pix_fmt;
    src.fmt.fmt.pix.field = V4L2_FIELD_ALTERNATE;

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
    device_init(V4L2_PIX_FMT_YUYV);
}

VIPObj::VIPObj(std::string dev_name, int w, int h, int pix_fmt, int num_buf,
  int type){
    default_parameters();
    m_dev_name = dev_name;
    src.width = w;
    src.height = h;
    src.num_buffers = num_buf;
    src.type=(v4l2_buf_type) type;
    //src.fmt.fmt.pix.pixelformat = pix_fmt;

    device_init(pix_fmt);
}

VIPObj::~VIPObj(){
    free(src.v4l2buf);
    close(m_fd);
    return;
}

/* In this example appliaction, user space allocates the buffers and
 * provides the buffer fd to be exported to the V4L2 driver
*/
bool VIPObj::request_buf(){
    struct v4l2_requestbuffers reqbuf;
    struct v4l2_buffer v4l2buf;
    int ret;

    reqbuf.type = src.type;
    reqbuf.memory = src.memory;
    reqbuf.count = src.num_buffers;

    ret = ioctl(m_fd, VIDIOC_REQBUFS, &reqbuf);
    if (ret < 0) {
        ERROR("VIDIOC_REQBUFS failed: %s (%d)", strerror(errno), ret);
        return false;
    }

    src.num_buffers = reqbuf.count;
    src.base_addr = (unsigned int **) calloc(src.num_buffers, sizeof(unsigned int));
    for (int i = 0; i < src.num_buffers; i++) {
        memset(&v4l2buf, 0, sizeof(v4l2buf));
        v4l2buf.type = src.type;
        v4l2buf.memory = src.memory;
        v4l2buf.length	= src.coplanar ? 2 : 1;
        v4l2buf.index = i;

        ret = ioctl(m_fd, VIDIOC_QUERYBUF, &v4l2buf);

        if (ret) {
            ERROR("VIDIOC_QUERYBUF failed: %s (%d)", strerror(errno), ret);
            return false;
        }

        src.base_addr[i] = (unsigned int *) mmap(NULL, v4l2buf.length, PROT_READ | PROT_WRITE,
			         MAP_SHARED, m_fd, v4l2buf.m.offset);

        if (MAP_FAILED == src.base_addr[i]) {
          while(i>=0){
            /* Unmap all previous buffers in case of failure*/
            i--;
            munmap(src.base_addr[i], src.size);
            src.base_addr[i] = NULL;
          }
          ERROR("Cant mmap buffers Y");
          return false;
        }
    }

    return true;
}

/*
* Queue V4L2 buffer
*/
bool VIPObj::queue_buf(int index){
    struct v4l2_buffer v4l2buf;
  	int			ret = -1;

  	memset(&v4l2buf,0,sizeof(v4l2buf));
  	v4l2buf.type	= src.type;
  	v4l2buf.memory	= src.memory;
  	v4l2buf.index	= index;
  	v4l2buf.field    = src.field;
  	v4l2buf.length	= 2;
  	gettimeofday(&v4l2buf.timestamp, NULL);

    ret = ioctl(m_fd, VIDIOC_QBUF, &v4l2buf);
    if (ret) {
        ERROR("VIDIOC_QBUF failed: %s (%d)", strerror(errno), ret);
    }

    return ret;
}

/*
* DeQueue V4L2 buffer
*/
int VIPObj::dequeue_buf(){
    struct v4l2_buffer v4l2buf;
    int ret;

    v4l2buf.type = src.type;
    v4l2buf.memory = src.memory;
    ret = ioctl(m_fd, VIDIOC_DQBUF, &v4l2buf);
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
