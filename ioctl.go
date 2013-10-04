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

func ptrToString(p unsafe.Pointer) string {
	return C.GoString((*C.char)(p))
}

func pixelFormat(format C.__u32) string {
	buf := new(bytes.Buffer)
	binary.Write(buf, binary.LittleEndian, format)
	return string(buf.Bytes()[:4])
}

func getCapabilities(fd uintptr) (DeviceCapabilities, error) {
	caps, err := C.webcam_capability(C.int(fd))

	if caps == nil {
		return DeviceCapabilities{}, errors.New("cannot allocate memory")
	} else {
		defer C.free(unsafe.Pointer(caps))
	}

	if err != nil {
		return DeviceCapabilities{}, err
	}

	var ret DeviceCapabilities

	ret.driver = ptrToString(unsafe.Pointer(&caps.driver[0]))
	ret.card = ptrToString(unsafe.Pointer(&caps.card[0]))
	ret.busInfo = ptrToString(unsafe.Pointer(&caps.bus_info[0]))
	ret.canCapture = bool(caps.capabilities&C.V4L2_CAP_VIDEO_CAPTURE != 0)

	return ret, nil
}

func getSupportedFormats(fd uintptr) ([]FormatDescription, error) {
	formats := make([]FormatDescription, 0, 16)

	for i := 0; ; i++ {
		format, err := C.webcam_supported_formats(C.int(fd), C.int(i))

		if format == nil {
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

		var dest FormatDescription

		//Convert 4-byte int to 4-byte string
		dest.pixelformat = pixelFormat(format.pixelformat)
		dest.description = ptrToString(unsafe.Pointer(&format.description[0]))
		formats = append(formats, dest)
	}
	return formats, nil
}

func resize(fd uintptr, width uint16, height uint16) (ImageFormat, error) {
	format, err := C.webcam_resize(C.int(fd), C.uint16_t(width), C.uint16_t(height))

	if format == nil {
		return ImageFormat{}, errors.New("cannot allocate memory")
	} else {
		defer C.free(unsafe.Pointer(format))
	}

	if err != nil {
		return ImageFormat{}, err
	}

	var result ImageFormat

	//Read union
	buf := bytes.NewBuffer(format.fmt[:])
	binary.Read(buf, binary.LittleEndian, &result.width)
	binary.Read(buf, binary.LittleEndian, &result.heigth)

	var pixfmt [4]byte
	binary.Read(buf, binary.LittleEndian, &pixfmt)
	result.pixelformat = string(pixfmt[:])

	buf.Next(4 + 4 + 4) //Skip field, bytesperline, sizeimage
	binary.Read(buf, binary.LittleEndian, &result.colorspace)

	return result, nil
}
