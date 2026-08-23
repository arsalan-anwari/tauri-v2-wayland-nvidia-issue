# Error 71 on Wayland + Nvidia: root cause

`Gdk-Message: Error 71 (Protocol error) dispatching to Wayland display`, reported against Tauri ([tauri#10702]), WebKitGTK ([webkit#280210]), GTK ([gtk#8056]) and NVIDIA egl-wayland ([egl-wayland#179]).

TLDR: I am asuming **this is not specific to WebKitGTK, and not specific to Tauri.** As it seems to reproduce in plain GTK3 code that calls the public GDK API `gdk_cairo_draw_from_gl()`. 

GTK3 begins a frame as a cairo/SHM frame, that API then creates the window's EGL surface *in the middle of the frame*, egl-wayland arms explicit sync on the surface as a result, and GTK3 finishes the frame by attaching its SHM buffer with no acquire point set. The compositor is correct to kill the client.

## Environment

Every measurement below is from one machine, using 5 runs.:

| | |
| --- | --- |
| GPU / driver | Nvidia RTX 3080, 610.57.04 (proprietary) |
| OS / kernel | Fedora 44, 7.1.9-200.fc44.x86_64 |
| Compositor | KDE Plasma 6.7.4, KWin, Wayland session |
| GTK | gtk3 3.24.52 |
| WebKitGTK | webkit2gtk4.1 2.52.5 |
| egl-wayland | 1.1.21 |

Other reporters on the upstream issues see the same failure on GNOME/Mutter and on wlroots compositors, with driver versions from 560 to 610, so the specific compositor does not seem appear to matter as long as it advertises `wp_linux_drm_syncobj_manager_v1`.

## Analysis

### The call path

Backtrace from `01-webkit`, breakpoint on `wl_egl_window_create`:

```
#0  wl_egl_window_create () at /lib64/libwayland-egl.so.1
#1  gdk_wayland_window_get_egl_surface () at /lib64/libgdk-3.so.0
#2  gdk_wayland_display_make_gl_context_current () at /lib64/libgdk-3.so.0
#3  gdk_gl_context_make_current () at /lib64/libgdk-3.so.0
#4  gdk_cairo_draw_from_gl () at /lib64/libgdk-3.so.0
#5  WebKit::AcceleratedBackingStore::Buffer::paint(_cairo*, WebCore::IntRect const&) const ()
#6  WebKit::AcceleratedBackingStore::paint(_cairo*, WebCore::IntRect const&) ()
#7  webkitWebViewBaseDraw(_GtkWidget*, _cairo*) ()
#8  gtk_widget_draw_internal () at /lib64/libgtk-3.so.0
...
#12 gtk_widget_render () at /lib64/libgtk-3.so.0
#13 gtk_main_do_event () at /lib64/libgtk-3.so.0
```

The EGL surface for the toplevel `wl_surface` is created by GDK, from inside `gdk_cairo_draw_from_gl()`, which is a public, documented GDK API whose entire purpose is compositing a GL texture into a cairo draw. WebKitGTK is a caller, but cannot root out the way its calling it as the cause as; `04-drawfromgl.c` produces the same backtrace with no WebKit loaded at all.

### Frame process

`gdk_window_begin_paint_internal()` decides what kind of frame to use (SHM or other) and at the start of the frame, whether the frame is drawn with GL or with cairo into a shared-memory buffer (see: `gdk/gdkwindow.c:2965` on `gtk-3-24`):

```c
window->current_paint.use_gl = window->impl_window->gl_paint_context != NULL;
```

On the first frame of a WebKitGTK window, `gl_paint_context` is still `NULL`, so the frame is an SHM frame. `gdk_cairo_draw_from_gl()` then call`gdk_window_get_paint_gl_context()` unconditionally (`gdk/gdkgl.c:357`), which creates that context and with it the EGL surface while the SHM frame is already in flight.

`gdk_window_end_paint_internal()` finishes the frame through the Wayland backend's `end_paint`, which attaches the SHM buffer precisely because the frame was not a GL frame (`gdk/wayland/gdkwindow-wayland.c:1084`):

```c
  if (impl->staging_cairo_surface &&
      _gdk_wayland_is_shm_surface (impl->staging_cairo_surface) &&
      !window->current_paint.use_gl &&
      !cairo_region_is_empty (window->current_paint.region))
    {
      gdk_wayland_window_attach_image (window);
```

`gdk_wayland_window_attach_image()` (`gdk/wayland/gdkwindow-wayland.c:904`) does a plain `wl_surface_attach` + commit. It has no notion of explicit sync, so no acquire point is ever set.

The Wayland backend's own GL code assumes this situation cannot arise (`gdk/wayland/gdkglcontext-wayland.c:52`):

```c
  /* Minimal update is ok if we're not drawing with gl */
  if (window->gl_paint_context == NULL)
    return;
```

That assumption that "no paint GL context means nothing GL is happening on this surface" is what I think `gdk_cairo_draw_from_gl()` breaks.

### The protocol trace

`04-drawfromgl` (crashes). SHM buffer allocated for the frame, EGL surface created mid-frame, SHM buffer attached, no acquire point:

```
-> wl_shm#7.create_pool(new id wl_shm_pool#36, fd 16, 1920000)
-> wl_shm_pool#36.create_buffer(new id wl_buffer#37, 0, 800, 600, 3200, 0)
...
-> wp_linux_drm_syncobj_manager_v1#39.get_surface(new id wp_linux_drm_syncobj_surface_v1#40, wl_surface#32)
-> wp_linux_drm_syncobj_manager_v1#39.import_timeline(new id wp_linux_drm_syncobj_timeline_v1#45, fd 39)
-> wl_surface#32.attach(wl_buffer#37, 0, 0)          <- SHM buffer, Default Queue
-> wl_surface#32.commit()
   wl_display#1.error(wp_linux_drm_syncobj_surface_v1#40, 4, "explicit sync is used, but no acquire point is set")
```

`05-earlygl` (survives). Same program, GL context created at `realize`, so the frame is a GL frame, no SHM buffer is ever attached, and egl-wayland attaches its own buffer with an acquire point:

```
-> wp_linux_drm_syncobj_manager_v1#34.get_surface(new id wp_linux_drm_syncobj_surface_v1#36, wl_surface#32)
-> wp_linux_drm_syncobj_manager_v1#34.import_timeline(new id wp_linux_drm_syncobj_timeline_v1#42, fd 39)
-> wp_linux_drm_syncobj_surface_v1#36.set_acquire_point(wp_linux_drm_syncobj_timeline_v1#42, 0, 1)
-> wl_surface#32.attach(wl_buffer#4278190080, 0, 0)  <- EGLSurface queue
-> wl_surface#32.commit()
```

Full traces in `traces/`.

### Why i think this is an Nvidia specific failure

Two clients drive one `wl_surface`: GDK (SHM) and egl-wayland (EGL/DMA-BUF). Nvidia's egl-wayland creates a `wp_linux_drm_syncobj_surface_v1` for every EGLSurface when the compositor advertises the manager, which arms explicit sync for the whole surface, including buffers GDK attaches behind its back. 

Mesa (as far as i am aware) does not opt the surface into explicit sync in this path, so the same illegal SHM attach goes unnoticed there. 

This also could explains some reports where the app works after a hot reload or works on some compositors. 

Only the first frame is an SHM frame and from frame two onward `gl_paint_context != NULL`, so `use_gl` is true and no SHM buffer is attached.

There could be a bug in `wp_linux_drm_syncobj_manager_v1` also but cannot confirm. Dont have time to investigate that path.

## Where it could be fixed

Realistically I see three independent places. Any one could be the culprit, and any one could fix the crash.

### 1. GTK3 (`gtk-3-24`)

The narrow fix is to not attach an SHM buffer to a surface that has become EGL-backed like suggested by the user of the original issue. (see reference in `patches/0001-gdk-wayland-do-not-attach-shm-buffer-to-egl-backed-su.patch`)

implements this in `gdk_wayland_window_attach_image()`, which covers both call sites (`end_paint` and `gdk_wayland_window_show`). The cost is that the frame
during which the EGL surface appeared shows the previous contents instead of the half-cairo frame, and the next frame a GL frame corrects it.

A larger fix would be to stop the situation arising at all: have `gdk_window_get_paint_gl_context()`, when it creates the context while `current_paint.surface` is non-NULL, promote the in-flight frame to a GL frame, or defer the EGL surface creation to the next frame boundary. That is more invasive than this bug probably justifies on a stable branch. I'd leave that open for your review. 

### 2. WebKitGTK

`AcceleratedBackingStore` currently creates its GDK GL context lazily from `paint()`. Realizing it when the web view widget is realized, before the first frame, avoids the mid-frame transition and fixes the crash without any GTK change. `06-webkit-earlygl.c` demonstrates the effect from outside the library.

### 3. Applications

Any GTK3 application can force the paint GL context to exist before the first frame. `gdk_window_create_gl_context()` creates it as a side effect (`gdk/gdkwindow.c:2919`) like this example:

```c
static void
force_gl_paint_context (GtkWidget *widget, gpointer data)
{
  GError *error = NULL;
  GdkGLContext *ctx = gdk_window_create_gl_context (gtk_widget_get_window (widget), &error);

  if (ctx == NULL)
    {
      g_printerr ("no gl context: %s\n", error->message);
      g_clear_error (&error);
      return;
    }
  g_object_unref (ctx);   // only the paint context it forced is needed
}
```

Verified in `06-webkit-earlygl.c`: all clean starts, DMA-BUF renderer active, `set_acquire_point` used correctly. 

This is worth having in Tauri, or anything else embedding WebKitGTK, because unlike the `WEBKIT_DISABLE_DMABUF_RENDERER=1` fix it keeps hardware acceleration, and unlike`__NV_DISABLE_EXPLICIT_SYNC=1` it does not push the driver onto the implicit sync path. But from my own app `kana-trainer` i am not really seeing a performance difference between the two paths, so it is not clear if this is worth the extra code complexity. I'd would have to be tested in a real app with a lot of GL content to see if it matters.

## Not tested

- Mesa hardware, to confirm the illegal attach is present but harmless there.
- Whether the same sequence exists in GTK4. GTK4's rendering model is different
  enough that it likely may not but many app developers are still on GTK3 and WebKitGTK is still GTK3-only, so this is the relevant path for now.

[tauri#10702]: https://github.com/tauri-apps/tauri/issues/10702
[webkit#280210]: https://bugs.webkit.org/show_bug.cgi?id=280210
[gtk#8056]: https://gitlab.gnome.org/GNOME/gtk/-/issues/8056
[egl-wayland#179]: https://github.com/NVIDIA/egl-wayland/issues/179
