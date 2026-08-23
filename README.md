# Error 71 on Wayland + Nvidia: isolated reproducers

Investigation of `Gdk-Message: Error 71 (Protocol error) dispatching to Wayland display`, the crash that stops WebKitGTK applications (Tauri, Photino, wxWidgets webview, GNOME apps embedding a webview) from starting on Nvidia GPUs under Wayland.

Upstream trackers:

- Tauri: <https://github.com/tauri-apps/tauri/issues/10702>
- WebKit: <https://bugs.webkit.org/show_bug.cgi?id=280210>
- GTK: <https://gitlab.gnome.org/GNOME/gtk/-/issues/8056>
- NVIDIA egl-wayland: <https://github.com/NVIDIA/egl-wayland/issues/179>

See [REPORT.md](REPORT.md) for the analysis. This README explains how to run it.

## Build and run

Needs `gtk3-devel`, `webkit2gtk4.1-devel`, `libepoxy-devel` and a C compiler.

```sh
./run.sh
```

Each reproducer opens a window, waits five seconds and exits 0 printing `survived`. A reproducer that hits the bug exits 1 with the GDK error instead. 

## Results

Measured on: 
- Nvidia RTX 3080, driver 610.57.04 
- Fedora 44 
- KDE Plasma 6.7.4(KWin, Wayland)
- gtk3 3.24.52
- webkit2gtk4.1 2.52.5
- egl-wayland 1.1.21.

Five runs each.

| Reproducer | What it does | Result |
| --- | --- | --- |
| `01-webkit.c` | `GtkWindow` + `WebKitWebView`, nothing else | **crashes** 5/5 |
| `02-glarea.c` | `GtkWindow` + `GtkGLArea`, no WebKit | survives 5/5 |
| `03-plain.c` | `GtkWindow` + `GtkLabel`, no GL at all | survives 5/5 |
| `04-drawfromgl.c` | `GtkWindow` calling `gdk_cairo_draw_from_gl()` from `draw`, no WebKit | **crashes** 5/5 |
| `05-earlygl.c` | same as 04, but the GL context is created at `realize` | survives 5/5 |
| `06-webkit-earlygl.c` | same as 01, plus `gdk_window_create_gl_context()` at `realize` | survives 5/5 |

> Note: `04` is plain GTK3 using a public GDK API, with no WebKit and it fails identically. 
> Note: `06` shows the crash can be avoided from application code today, without disabling hardware acceleration.
