/* Same window lifecycle, GL on the toplevel via GtkGLArea, no WebKit used.
 * Build: gcc -o 02-glarea 02-glarea.c $(pkg-config --cflags --libs gtk+-3.0 epoxy)
 */
#include <gtk/gtk.h>
#include <epoxy/gl.h>

static gboolean quit_cb (gpointer data) { gtk_main_quit (); return G_SOURCE_REMOVE; }

static gboolean render_cb (GtkGLArea *area, GdkGLContext *ctx, gpointer data)
{
  glClearColor (0.2f, 0.4f, 0.8f, 1.0f);
  glClear (GL_COLOR_BUFFER_BIT);
  return TRUE;
}

int main (int argc, char **argv)
{
  gtk_init (&argc, &argv);

  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  GtkWidget *area = gtk_gl_area_new ();
  g_signal_connect (area, "render", G_CALLBACK (render_cb), NULL);
  gtk_container_add (GTK_CONTAINER (window), area);

  gtk_widget_show_all (window);
  g_timeout_add_seconds (5, quit_cb, NULL);
  gtk_main ();
  g_print ("survived\n");
  return 0;
}
