/* 01-webkit.c plus the candidate application-side fix.
 *
 * gdk_window_create_gl_context() creates the window's paint GL context as a
 * side effect. Doing it at realize time, before the first frame, means
 * begin_paint sees gl_paint_context != NULL and runs the whole frame on GL, so
 * GTK never allocates an SHM buffer for this surface and never attaches one to
 * a surface that egl-wayland has armed with explicit sync.
 *
 * Build: gcc -o 06-webkit-earlygl 06-webkit-earlygl.c $(pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.1)
 */
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

static gboolean quit_cb (gpointer data) { gtk_main_quit (); return G_SOURCE_REMOVE; }

static void force_gl_paint_context (GtkWidget *widget, gpointer data)
{
  GError *error = NULL;
  GdkWindow *window = gtk_widget_get_window (widget);
  GdkGLContext *ctx = gdk_window_create_gl_context (window, &error);

  if (ctx == NULL)
    {
      g_printerr ("no gl context: %s\n", error->message);
      g_clear_error (&error);
      return;
    }
  /* The context itself is not needed, only the paint context it forced GDK to create so it can be dropped again. */
  g_object_unref (ctx);
}

int main (int argc, char **argv)
{
  gtk_init (&argc, &argv);

  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (window, "realize", G_CALLBACK (force_gl_paint_context), NULL);

  GtkWidget *view = webkit_web_view_new ();
  gtk_container_add (GTK_CONTAINER (window), view);
  webkit_web_view_load_html (WEBKIT_WEB_VIEW (view), "<html><body>hello</body></html>", NULL);

  gtk_widget_show_all (window);
  g_timeout_add_seconds (5, quit_cb, NULL);
  gtk_main ();
  g_print ("survived\n");
  return 0;
}
