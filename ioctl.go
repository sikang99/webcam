package webcam

// #include "webcam.h"
// #include <linux/videodev2.h>
// #include <stdlib.h>
import "C"
import (
	"errors"
	"unsafe"
)

type deviceCapabilities struct {
	driver     string
	card       string
	busInfo    string
	version    uint32
	canCapture bool
}

func getCapabilities(fd uintptr) (deviceCapabilities, error) {
	caps, err := C.webcam_capability(C.int(fd))

	if unsafe.Pointer(caps) == unsafe.Pointer(uintptr(0)) {
		return deviceCapabilities{}, errors.New("cannot allocate memory")
	} else {
		defer C.free(unsafe.Pointer(caps))
	}

	if err != nil {
		return deviceCapabilities{}, err
	}

	var ret deviceCapabilities

	ret.driver = C.GoString((*C.char)(unsafe.Pointer(&caps.driver[0])))
	ret.card = C.GoString((*C.char)(unsafe.Pointer(&caps.card[0])))
	ret.busInfo = C.GoString((*C.char)(unsafe.Pointer(&caps.bus_info[0])))
	ret.canCapture = bool(caps.capabilities&C.V4L2_CAP_VIDEO_CAPTURE != 0)

	return ret, nil
}
