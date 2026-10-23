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

### 0003 - report panel-derived DPI to KWin's own QPA

KWin's QPA screen implementation normally returns `QDpi(96, 96)`.  The R11T
panel is physically dense, so `0003` computes logical DPI from the DRM
connector dimensions:

```text
hypot(1080, 1920) / (hypot(68, 122) / 25.4) = 401 DPI
```

`QT_WAYLAND_FORCE_DPI` still takes precedence, and invalid or missing physical
dimensions fall back to 96 DPI.  The patch keeps the output at the native
`1080x1920` mode and does not change the output scale.

Scope matters: `0003` changes the QPA of the `kwin_wayland` process only.  It
does not set DPI for `plasmashell`, Konsole, or other independent Wayland
clients, and it does not enlarge their buttons or layouts.  On Qt 6.12,
`QT_FONT_DPI` and `QT_WAYLAND_FORCE_DPI` enlarge fonts only when
`QT_ENABLE_HIGHDPI_SCALING=0`; with HighDPI enabled they change the client DPR
as well.  Making Qt geometry and controls larger therefore requires
client-side HighDPI scaling (`QT_SCALE_FACTOR`, per-screen scale factors, or an
equivalent compositor/output scale).  The KWin patch is not a system-wide DPI
switch.

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
