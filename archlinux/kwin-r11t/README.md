# archlinux/kwin-r11t

Locally patched build of the Plasma compositor for the OPPO R11T port.

| file | purpose |
| --- | --- |
| `PKGBUILD` | `kwin` 6.7.5, `arch=(aarch64)`, `pkgrel=3.4`, applies the patches in `prepare()` |
| `0001-kwin-drm-EBUSY-retry-on-next-vblank.patch` | DRM atomic-commit `EBUSY` retry + saved `errno` |
| `0002-kwin-screenshot-make-context-current.patch` | screenshot: make the target's OpenGL context current before painting (upstream backport) |
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

Because the `errno` is read *after* the collapse retry, the logged error can
even describe a different failure than the one that actually happened.

The patch:

1. captures `errno` immediately after `commit()` returns, before any retry can
   overwrite it, and uses it for the warning;
2. keeps an `EBUSY` commit queued and waits for at least one full vblank (up to
   ~500 ms total) before retrying, then tries repeatedly with a full vblank
   gap - this recovers when the previous commit's worker is still running on
   the MSM driver (a case where a single retry is insufficient);
3. resets the retry counter after a successful commit and only logs a warning
   if the retry budget is exhausted.

### 0002 - screenshot OpenGL context (crash fix)

With a QML based effect loaded (the built in `showfps` is the easy example),
*every* `ScreenShot2` request - `CaptureArea`, `CaptureScreen`,
`CaptureActiveScreen`, `CaptureWorkspace`, and therefore also Spectacle and the
`xdg-desktop-portal` screenshot dialog - crashed `kwin_wayland` with `SIGSEGV`
in `EglContext::currentFramebuffer()`.  The compositor stayed up while idle and
only died when a capture was requested.

Chain of events:

1. `ScreenShotManager::takeScreenShot()` builds a `SceneView` for the capture
   and calls `sceneView.prePaint()`.
2. `WorkspaceScene::prePaint()` calls `effects->prePaintScreen()`.  A QML effect
   that has no `OffscreenQuickScene` for the new render view constructs one, and
   `OffscreenQuickView`'s constructor makes its own `QOpenGLContext` current and
   then calls `doneCurrent()`.
3. That `doneCurrent()` runs through the QPA `EGLPlatformContext` into KWin's
   `EglContext::doneCurrent()`, which sets the process wide
   `EglContext::s_currentContext` to `nullptr`, and nothing restores it.
4. `sceneView.paint()` -> `ItemRendererOpenGL::beginFrame()` ->
   `GLFramebuffer::pushFramebuffer()` dereferences that null current context.

The DRM backend never hits this because `EglGbmLayerSurface::startRendering()`
makes the compositor's context current before each composite.  The screenshot
layer did not, and its `beginFrame()` also ran *before* `prePaint()`.

The patch is the upstream fix (KDE/kwin `master`, `screenshot.cpp` +
`screenshotlayer.cpp` + the new `GLFramebuffer::context()` accessor):

* `ScreenshotLayer::doBeginFrame()` makes the target framebuffer's context
  current and fails cleanly if that is not possible;
* both `SceneView` based `takeScreenShot()` overloads now call
  `layer.beginFrame()` *after* `sceneView.prePaint()`, so the `makeCurrent()`
  happens after the effects have had their chance to switch the context.

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
sudo pacman -U kwin-6.7.5-3.4-aarch64.pkg.tar.zst
sudo systemctl restart sddm.service
```

Rollback:

```bash
sudo pacman -U /root/kwin-<oldversion>.pkg.tar.zst
```
