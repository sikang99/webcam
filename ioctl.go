package webcam

// #include "webcam.h"
// #include <linux/videodev2.h>
// #include <stdlib.h>
import "C"
import (
	"bytes"
	"encoding/binary"
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

type supportedFormat struct {
	format      string
	description string
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

func getSupportedFormats(fd uintptr) ([]supportedFormat, error) {
	formats := make([]supportedFormat, 0, 16)

	for i := 0; ; i++ {
		format, err := C.webcam_supported_formats(C.int(fd), C.int(i))

		if unsafe.Pointer(format) == unsafe.Pointer(uintptr(0)) {
			return formats, errors.New("cannot allocate memory")
		} else {
			defer C.free(unsafe.Pointer(format))
		}

		if err != nil {
			if len(formats) == 0 {
				return formats, err
			} else {
				break
			}
		}

		var dest supportedFormat

		//Convert 4-byte int to 4-byte string
		buf := new(bytes.Buffer)
		binary.Write(buf, binary.LittleEndian, format.pixelformat)
		dest.format = string(buf.Bytes()[:4])

		dest.description = C.GoString((*C.char)(unsafe.Pointer(&format.description[0])))
		formats = append(formats, dest)
	}
	return formats, nil
}
