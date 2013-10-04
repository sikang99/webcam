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


typedef struct buffer {
    uint8_t *start;
    size_t length;
} buffer_t;

/**
 * Webcam structure
 */
typedef struct webcam {
    char *name;
    int fd;
    buffer_t *buffers;
    uint8_t nbuffers;

    buffer_t frame;
    pthread_t thread;
    pthread_mutex_t mtx_frame;

    uint16_t width;
    uint16_t height;
    uint8_t colorspace;

    char formats[16][5];
    bool streaming;
} webcam_t;


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

/**
 * Private function to clamp a double value to the nearest int
 * between 0 and 255
 */
static uint8_t clamp(double x)
{
    int r = x;

    if (r < 0) return 0;
    else if (r > 255) return 255;

    return r;
}

/**
 * Private function to convert a YUYV buffer to a RGB frame and store it
 * within the given buffer structure
 *
 * http://linuxtv.org/downloads/v4l-dvb-apis/colorspaces.html
 */
static void convertToRGB(struct buffer buf, struct buffer *frame)
{
    size_t i;
    uint8_t y, u, v;

    int uOffset = 0;
    int vOffset = 0;

    double R, G, B;
    double Y, Pb, Pr;

    // Initialize frame
    if (frame->start == NULL) {
        frame->length = buf.length / 2 * 3;
        frame->start = calloc(frame->length, sizeof(char));
    }

    // Go through the YUYV buffer and calculate RGB pixels
    for (i = 0; i < buf.length; i += 2)
    {
        uOffset = (i % 4 == 0) ? 1 : -1;
        vOffset = (i % 4 == 2) ? 1 : -1;

        y = buf.start[i];
        u = (i + uOffset > 0 && i + uOffset < buf.length) ? buf.start[i + uOffset] : 0x80;
        v = (i + vOffset > 0 && i + vOffset < buf.length) ? buf.start[i + vOffset] : 0x80;

        Y =  (255.0 / 219.0) * (y - 0x10);
        Pb = (255.0 / 224.0) * (u - 0x80);
        Pr = (255.0 / 224.0) * (v - 0x80);

        R = 1.0 * Y + 0.000 * Pb + 1.402 * Pr;
        G = 1.0 * Y + 0.344 * Pb - 0.714 * Pr;
        B = 1.0 * Y + 1.772 * Pb + 0.000 * Pr;

        frame->start[i / 2 * 3    ] = clamp(R);
        frame->start[i / 2 * 3 + 1] = clamp(G);
        frame->start[i / 2 * 3 + 2] = clamp(B);
    }
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

/**
 * Open the webcam on the given device and return a webcam
 * structure.
 */
/*struct webcam *webcam_open(const char *dev)*/
/*{*/
    /*struct stat st;*/

    /*struct v4l2_format fmt;*/

    /*uint16_t min;*/

    /*int fd;*/
    /*struct webcam *w;*/



    /*// Request supported formats*/
    /*struct v4l2_fmtdesc fmtdesc;*/
    /*uint32_t idx = 0;*/
    /*char *pixelformat = calloc(5, sizeof(char));*/
    /*for(;;) {*/
        /*fmtdesc.index = idx;*/
        /*fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;*/

        /*if (-1 == _ioctl(w->fd, VIDIOC_ENUM_FMT, &fmtdesc)) break;*/

        /*memset(w->formats[idx], 0, 5);*/
        /*memcpy(&w->formats[idx][0], &fmtdesc.pixelformat, 4);*/
        /*fprintf(stderr, "%s: Found format: %s - %s\n", w->name, w->formats[idx], fmtdesc.description);*/
        /*idx++;*/
    /*}*/

    /*return w;*/
/*}*/

/**
 * Closes the webcam
 *
 * Also releases the buffers, and frees up memory
 */
void webcam_close(webcam_t *w)
{
    uint16_t i;

    // Clear frame
    free(w->frame.start);
    w->frame.length = 0;

    // Release memory-mapped buffers
    for (i = 0; i < w->nbuffers; i++) {
        munmap(w->buffers[i].start, w->buffers[i].length);
    }

    // Free allocated resources
    free(w->buffers);
    free(w->name);

    // Close the webcam file descriptor, and free the memory
    close(w->fd);
    free(w);
}

/**
 * Sets the webcam to capture at the given width and height
 */
void webcam_resize(webcam_t *w, uint16_t width, uint16_t height)
{
    uint32_t i;
    struct v4l2_format fmt;
    struct v4l2_buffer buf;

    // Use YUYV as default for now
    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.colorspace = V4L2_COLORSPACE_REC709;
    fprintf(stderr, "%s: requesting image format %ux%u\n", w->name, width, height);
    _ioctl(w->fd, VIDIOC_S_FMT, &fmt);

    // Storing result
    w->width = fmt.fmt.pix.width;
    w->height = fmt.fmt.pix.height;
    w->colorspace = fmt.fmt.pix.colorspace;

    char *pixelformat = calloc(5, sizeof(char));
    memcpy(pixelformat, &fmt.fmt.pix.pixelformat, 4);
    fprintf(stderr, "%s: set image format to %ux%u using %s\n", w->name, w->width, w->height, pixelformat);

    // Buffers have been created before, so clear them
    if (NULL != w->buffers) {
        for (i = 0; i < w->nbuffers; i++) {
            munmap(w->buffers[i].start, w->buffers[i].length);
        }

        free(w->buffers);
    }

    // Request the webcam's buffers for memory-mapping
    struct v4l2_requestbuffers req;
    CLEAR(req);

    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == _ioctl(w->fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s does not support memory mapping\n", w->name);
            return;
        } else {
            fprintf(stderr, "Unknown error with VIDIOC_REQBUFS: %d\n", errno);
            return;
        }
    }

    // Needs at least 2 buffers
    if (req.count < 2) {
        fprintf(stderr, "Insufficient buffer memory on %s\n", w->name);
        return;
    }

    // Storing buffers in webcam structure
    fprintf(stderr, "Preparing %d buffers for %s\n", req.count, w->name);
    w->nbuffers = req.count;
    w->buffers = calloc(w->nbuffers, sizeof(struct buffer));

    if (!w->buffers) {
        fprintf(stderr, "Out of memory\n");
        return;
    }

    // Prepare buffers to be memory-mapped
    for (i = 0; i < w->nbuffers; ++i) {
        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (-1 == _ioctl(w->fd, VIDIOC_QUERYBUF, &buf)) {
            fprintf(stderr, "Could not query buffers on %s\n", w->name);
            return;
        }

        w->buffers[i].length = buf.length;
        w->buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, w->fd, buf.m.offset);

        if (MAP_FAILED == w->buffers[i].start) {
            fprintf(stderr, "Mmap failed\n");
            return;
        }
    }
}

/**
 * Reads a frame from the webcam, converts it into the RGB colorspace
 * and stores it in the webcam structure
 */
static void webcam_read(struct webcam *w)
{
    struct v4l2_buffer buf;

    // Try getting an image from the device
    for(;;) {
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        // Dequeue a (filled) buffer from the video device
        if (-1 == _ioctl(w->fd, VIDIOC_DQBUF, &buf)) {
            switch(errno) {
                case EAGAIN:
                    continue;

                case EIO:
                default:
                    fprintf(stderr, "%d: Could not read from device %s\n", errno, w->name);
                    break;
            }
        }

        // Make sure we are not out of bounds
        assert(buf.index < w->nbuffers);

        // Lock frame mutex, and store RGB
        pthread_mutex_lock(&w->mtx_frame);
        convertToRGB(w->buffers[buf.index], &w->frame);
        pthread_mutex_unlock(&w->mtx_frame);
        break;
    }

    // Queue buffer back into the video device
    if (-1 == _ioctl(w->fd, VIDIOC_QBUF, &buf)) {
        fprintf(stderr, "Error while swapping buffers on %s\n", w->name);
        return;
    }
}

/**
 * The loop function for the webcam thread
 */
static void *webcam_streaming(void *ptr)
{
    webcam_t *w = (webcam_t *)ptr;

    while(w->streaming) webcam_read(w);

    return NULL;
}

/**
 * Tells the webcam to go into streaming mode, or to
 * stop streaming.
 * When going into streaming mode, it also creates
 * a thread running the webcam_streaming function.
 * When exiting the streaming mode, it sets the streaming
 * bit to false, and waits for the thread to finish.
 */
void webcam_stream(struct webcam *w, bool flag)
{
    uint8_t i;

    struct v4l2_buffer buf;
    enum v4l2_buf_type type;

    if (flag) {
        // Clear buffers
        for (i = 0; i < w->nbuffers; i++) {
            CLEAR(buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (-1 == _ioctl(w->fd, VIDIOC_QBUF, &buf)) {
                fprintf(stderr, "Error clearing buffers on %s\n", w->name);
                return;
            }
        }

        // Turn on streaming
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == _ioctl(w->fd, VIDIOC_STREAMON, &type)) {
            fprintf(stderr, "Could not turn on streaming on %s\n", w->name);
            return;
        }

        // Set streaming to true and start thread
        w->streaming = true;
        pthread_create(&w->thread, NULL, webcam_streaming, (void *)w);
    } else {
        // Set streaming to false and wait for thread to finish
        w->streaming = false;
        pthread_join(w->thread, NULL);

        // Turn off streaming
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == _ioctl(w->fd, VIDIOC_STREAMOFF, &type)) {
            fprintf(stderr, "Could not turn streaming off on %s\n", w->name);
            return;
        }
    }
}

void webcam_grab(webcam_t *w, buffer_t *frame)
{
    // Locks the frame mutex so the grabber can copy
    // the frame in its own return buffer.
    pthread_mutex_lock(&w->mtx_frame);

    // Only copy frame if there is something in the webcam's frame buffer
    if (w->frame.length > 0) {
        // Initialize frame
        if ((*frame).start == NULL) {
            (*frame).start = calloc(w->frame.length, sizeof(char));
            (*frame).length = w->frame.length;
        }

        memcpy((*frame).start, w->frame.start, w->frame.length);
    }

    pthread_mutex_unlock(&w->mtx_frame);
}

/**
 * Main code
 */
#ifdef WEBCAM_TEST
int main(int argc, char **argv)
{
    int i = 0;
    webcam_t *w = webcam_open("/dev/video0");

    // Prepare frame, and filename, and file to store frame in
    buffer_t frame;
    frame.start = NULL;
    frame.length = 0;

    char *fn = calloc(16, sizeof(char));
    FILE *fp;

    webcam_resize(w, 640, 480);
    webcam_stream(w, true);
    while(true) {
        webcam_grab(w, &frame);

        if (frame.length > 0) {
            printf("Storing frame %d\n", i);
            sprintf(fn, "frame_%d.rgb", i);
            fp = fopen(fn, "w+");
            fwrite(frame.start, frame.length, 1, fp);
            fclose(fp);
            i++;
        }

        if (i > 10) break;
    }
    webcam_stream(w, false);
    webcam_close(w);

    if (frame.start != NULL) free(frame.start);
    free(fn);

    return 0;
}
#endif

#endif //GO_WEBCAM_H
