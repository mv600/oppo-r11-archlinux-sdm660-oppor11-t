# Venus VA-API driver for R11T

The R11T uses Qualcomm Venus through stateful V4L2 M2M. Mesa does not provide
a VA-API driver for this path, so `vainfo` fails with error code -1 and
applications that require VA-API cannot initialize.

This package builds the pinned `snowf14k3/venus-vaapi-driver` commit and
installs `/usr/lib/dri/venus_drv_video.so`. It is an independent userspace
backend, not part of Mesa or the kernel.

Validate before enabling it system-wide:

```sh
LIBVA_DRIVER_NAME=venus vainfo --display drm --device /dev/dri/renderD128
```

The driver currently supports H.264, VP8 and VP9 decode. HEVC decode is an
upstream experimental opt-in and remains disabled by default.
