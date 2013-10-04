package webcam

type DeviceCapabilities struct {
	driver     string
	card       string
	busInfo    string
	version    uint32
	canCapture bool
}

type FormatDescription struct {
	pixelformat string
	description string
}

type ImageFormat struct {
	width       uint32
	heigth      uint32
	pixelformat string
	colorspace  Colorspace
}

// Corresponding enum v4l2_colorspace from <linux/videodev2.h>
type Colorspace uint32

const (
	/* ITU-R 601 -- broadcast NTSC/PAL */
	V4L2_COLORSPACE_SMPTE170M Colorspace = iota

	/* 1125-Line (US) HDTV */
	V4L2_COLORSPACE_SMPTE240M

	/* HD and modern captures. */
	V4L2_COLORSPACE_REC709

	/* broken BT878 extents (601, luma range 16-253 instead of 16-235) */
	V4L2_COLORSPACE_BT878

	/* These should be useful.  Assume 601 extents. */
	V4L2_COLORSPACE_470_SYSTEM_M
	V4L2_COLORSPACE_470_SYSTEM_BG

	/* I know there will be cameras that send this.  So, this is
	 * unspecified chromaticities and full 0-255 on each of the
	 * Y'CbCr components
	 */
	V4L2_COLORSPACE_JPEG

	/* For RGB colourspaces, this is probably a good start. */
	V4L2_COLORSPACE_SRGB
)
