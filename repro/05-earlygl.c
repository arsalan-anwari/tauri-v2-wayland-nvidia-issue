/* Pure GTK3 reproducer. No WebKit, no Tauri.
 *
 * Identical to 04, except the GL context is realized and made
 * current at realize time, before the first frame, so GDK creates the EGL
 * surface before begin_paint decides the frame is an SHM frame.
 *
 * Build: gcc -o 05-earlygl 04-drawfromgl.c $(pkg-config --cflags --libs gtk+-3.0 epoxy)
 */
#include <gtk/gtk.h>
#include <epoxy/gl.h>

static GdkGLContext *ctx = NULL;
static guint texture = 0;

static gboolean quit_cb (gpointer data) { gtk_main_quit (); return G_SOURCE_REMOVE; }

static void setup_gl (GtkWidget *widget, gpointer data)
{
  GdkWindow *window = gtk_widget_get_window (widget);
    {
      GError *error = NULL;
      ctx = gdk_window_create_gl_context (window, &error);
      if (ctx == NULL) { g_printerr ("no gl context: %s\n", error->message); return; }
      if (!gdk_gl_context_realize (ctx, &error)) { g_printerr ("realize: %s\n", error->message); return; }
      gdk_gl_context_make_current (ctx);

      guint32 *pixels = g_malloc0 (64 * 64 * 4);
      for (int i = 0; i < 64 * 64; i++) pixels[i] = 0xff3366ccu;
      glGenTextures (1, &texture);
      glBindTexture (GL_TEXTURE_2D, texture);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
      g_free (pixels);
    }
}

static gboolean draw_cb (GtkWidget *widget, cairo_t *cr, gpointer data)
{
  int w = gtk_widget_get_allocated_width (widget);
  int h = gtk_widget_get_allocated_height (widget);

  gdk_cairo_draw_from_gl (cr, gtk_widget_get_window (widget), texture, GL_TEXTURE, 1, 0, 0, w, h);
  return FALSE;
}

int main (int argc, char **argv)
{
  gtk_init (&argc, &argv);

  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (window, "realize", G_CALLBACK (setup_gl), NULL);
  g_signal_connect (window, "draw", G_CALLBACK (draw_cb), NULL);

  gtk_widget_show_all (window);
  g_timeout_add_seconds (5, quit_cb, NULL);
  gtk_main ();
  g_print ("survived\n");
  return 0;
}
