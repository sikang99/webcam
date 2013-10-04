#ifndef GO_WEBCAM_H
#define GO_WEBCAM_H

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <linux/videodev2.h>


struct buffer_t {
    uint8_t *start;
    size_t length;
};


#define CLEAR(x) memset(&(x), 0, sizeof(x))

/**
 * Private function for successfully ioctl-ing the v4l2 device
 */
static int _ioctl(int fh, int request, void *arg)
{
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

struct v4l2_capability *webcam_capability(int fd)
{
    typedef struct v4l2_capability capability;
    capability *cap = calloc(1,sizeof(capability));
    if (cap == NULL)
        return cap;

    _ioctl(fd, VIDIOC_QUERYCAP, cap);
    return cap;
}

struct v4l2_fmtdesc* webcam_supported_formats(int fd, int idx)
{
    typedef struct v4l2_fmtdesc fmtdesc;
    fmtdesc *format = calloc(1,sizeof(fmtdesc));

    format->index = idx;
    format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    _ioctl(fd, VIDIOC_ENUM_FMT, format);
    return format;
}

struct v4l2_format* webcam_resize(int fd, uint16_t width, uint16_t height)
{
    typedef struct v4l2_format format;
    format *fmt = calloc(1,sizeof(format));
    if (fmt==NULL) {
        return fmt;
    }

    // Use YUYV as default for now
    fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt->fmt.pix.width = width;
    fmt->fmt.pix.height = height;
    fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt->fmt.pix.colorspace = V4L2_COLORSPACE_REC709;

    _ioctl(fd, VIDIOC_S_FMT, fmt);

    return fmt;
}

struct buffer_t webcam_query_buffer(int fd)
{
    struct buffer_t data;
    CLEAR(data);

    // Request the webcam's buffers for memory-mapping
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == _ioctl(fd, VIDIOC_REQBUFS, &req))
        return data;

    struct v4l2_buffer buf = {0};
    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if (-1 == _ioctl(fd, VIDIOC_QUERYBUF, &buf))
        return data;

    data.length = buf.length;
    data.start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

    return data;
}

/**
 * Reads a frame from the webcam, converts it into the RGB colorspace
 * and stores it in the webcam structure
 */
char* webcam_start_streaming(int fd)
{
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == _ioctl(fd, VIDIOC_STREAMON, &type))
        return "could not start capturing data";
    else
        return NULL;
}

char* webcam_stop_streaming(int fd)
{
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == _ioctl(fd, VIDIOC_STREAMOFF, &type))
        return "could not stop capturing data";
    else
        return NULL;
}

char* webcam_read(int fd)
{
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(-1 == _ioctl(fd, VIDIOC_QBUF, &buf))
        return "error querying buffer";

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv = {0};
    tv.tv_sec = 2;
    int r = select(fd+1, &fds, NULL, NULL, &tv);
    if(-1 == r)
        return "error waiting for frame";

    if(-1 == _ioctl(fd, VIDIOC_DQBUF, &buf))
        return "error retrieving frame";

    return NULL;
}

#endif //GO_WEBCAM_H
