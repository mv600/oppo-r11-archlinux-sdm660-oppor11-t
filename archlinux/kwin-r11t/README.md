# archlinux/kwin-r11t

Locally patched build of the Plasma compositor for the OPPO R11T port.

| file | purpose |
| --- | --- |
| `PKGBUILD` | `kwin` 6.8.0, `arch=(aarch64)`, `pkgrel=1.1`, applies the patches in `prepare()` |
| `0001-kwin-drm-EBUSY-retry-on-next-vblank.patch` | DRM atomic-commit `EBUSY` retry |
| `0003-kwin-qpa-use-panel-physical-dpi.patch` | QPA: derive Qt logical DPI from the panel mm size while keeping native 1080x1920 scanout |
| `build-kwin.yml` | copy of `.github/workflows/build-kwin.yml` for reference |

## Why the patches exist

### 0001 - DRM atomic-commit `EBUSY` retry

`sdc_plane_atomic_check` / `msm_atomic_commit` on the downstream `msm` DRM
driver can return `-EBUSY` while the display pipeline is still draining a
previous commit.  `DrmCommitThread::commitPending()` treats every failure the
same way: it collapses the pending commits, and if the collapse also fails it
drops them and logs

```text
atomic commit failed: 设备或资源忙
```

6.8.0 reports failures as `std::expected<void, OutputError>`, so the patch
matches the returned `InvalidApiUsage`/`EBUSY` error instead of reading a
process-global `errno` after a retry.

The patch:

1. recognizes the returned `EBUSY` error without depending on mutable global
   error state;
2. keeps an `EBUSY` commit queued and waits for at least one full vblank (up to
   ~500 ms total) before retrying, then tries repeatedly with a full vblank
   gap - this recovers when the previous commit's worker is still running on
   the MSM driver (a case where a single retry is insufficient);
3. resets the retry counter after a successful commit and only logs a warning
   if the retry budget is exhausted.

### 0003 - native-pixel output with panel-derived DPI

KWin's QPA screen implementation previously returned `QDpi(96, 96)` whenever
`QT_WAYLAND_FORCE_DPI` was unset.  On the R11T panel that makes a 5.5-inch
1080p screen look like a low-density desktop: fonts, decorations and controls
are physically too small even though the framebuffer is correct.

`0003` keeps the compositor output at the native mode size (`1080x1920`,
`scale=1`) and changes only the Qt logical DPI.  It computes DPI from the
panel geometry exposed by the DRM connector:

```text
hypot(1080, 1920) / (hypot(68, 122) / 25.4) = 401 DPI
```

The explicit `QT_WAYLAND_FORCE_DPI` override remains the first choice, and
invalid or missing physical dimensions fall back to 96 DPI.  This avoids
full-screen scale factors and their resampling cost; fonts and controls are
sized from the actual panel instead.

## Building

Handled by `.github/workflows/build-kwin.yml` on an `aarch64` runner using the
`menci/archlinuxarm:base-devel` container, mirroring the working
`build-power-profiles-daemon.yml` workflow.

## Installing

See the `INSTALL.md` generated next to the artifact.  Keep the currently
installed `kwin` package file for rollback:

```bash
pacman -Q kwin
sudo cp /var/cache/pacman/pkg/kwin-*.pkg.tar.zst /root/
sudo pacman -U kwin-6.8.0-1.1-aarch64.pkg.tar.zst
sudo systemctl restart sddm.service
```

Rollback:

```bash
sudo pacman -U /root/kwin-<oldversion>.pkg.tar.zst
```
