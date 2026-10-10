# archlinux/kwin-r11t

Locally patched build of the Plasma compositor for the OPPO R11T port.

| file | purpose |
| --- | --- |
| `PKGBUILD` | `kwin` 6.7.5, `arch=(aarch64)`, `pkgrel=3.2`, applies the patch in `prepare()` |
| `0001-kwin-drm-EBUSY-retry-on-next-vblank.patch` | DRM atomic-commit `EBUSY` retry + saved `errno` |
| `build-kwin.yml` | copy of `.github/workflows/build-kwin.yml` for reference |

## Why the patch exists

`sdc_plane_atomic_check` / `msm_atomic_commit` on the downstream `msm` DRM
driver can return `-EBUSY` while the display pipeline is still draining a
previous commit.  `DrmCommitThread::commitPending()` treats every failure the
same way: it collapses the pending commits, and if the collapse also fails it
drops them and logs

```text
atomic commit failed: 设备或资源忙
```

Because the `errno` is read *after* the collapse retry, the logged error can
even describe a different failure than the one that actually happened.

The patch:

1. captures `errno` immediately after `commit()` returns, before any retry can
   overwrite it, and uses it for the warning;
2. keeps an `EBUSY` commit queued and advances its target pageflip time to the
   next vblank, retrying for up to ten attempts before falling back to the
   existing collapse path;
3. resets the retry counter after a successful commit.

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
sudo pacman -U kwin-6.7.5-3.2-aarch64.pkg.tar.zst
sudo systemctl restart sddm.service
```

Rollback:

```bash
sudo pacman -U /root/kwin-<oldversion>.pkg.tar.zst
```
