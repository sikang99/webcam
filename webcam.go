package webcam

import (
	"fmt"
	"os"
	"syscall"
)

type WebcamError struct {
	device    string
	operation string
	err       string
}

func (e WebcamError) Error() string {
	return fmt.Sprintf("Error %s %s: %s", e.operation, e.device, e.err)
}

type Webcam struct {
	file os.File
}

func Open(path string) (Webcam, error) {
	info, err := os.Stat(path)
	if err != nil {
		return Webcam{}, WebcamError{path, "opening", "path not exists"}
	}

	if info.Mode()&os.ModeCharDevice == 0 {
		return Webcam{}, WebcamError{path, "opening", "not a device"}
	}

	file, err := os.OpenFile(path, os.O_RDWR|syscall.O_NONBLOCK, 0)
	if err != nil {
		e, _ := err.(*os.PathError)
		return Webcam{}, WebcamError{path, "opening", e.Err.Error()}
	}

	caps, err := getCapabilities(file.Fd())
	if err != nil {
		return Webcam{}, WebcamError{path, "getting capabilities of", err.Error()}
	}

	if !caps.canCapture {
		return Webcam{}, WebcamError{path, "opening", "not a capturing device"}
	}

	fmt.Printf("Capabilities: %+v\n", caps)

	formats, err := getSupportedFormats(file.Fd())
	if err != nil {
		return Webcam{}, WebcamError{path, "getting supported formats from", err.Error()}
	}
	fmt.Printf("Formats: %+v\n", formats)

	format, err := resize(file.Fd(), 640, 480)
	if err != nil {
		return Webcam{}, WebcamError{path, "resizing", err.Error()}
	}
	fmt.Printf("Resized to format: %+v\n", format)

	return Webcam{}, nil
}
