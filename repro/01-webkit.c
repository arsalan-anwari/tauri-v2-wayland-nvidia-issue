/* Minimal WebKitGTK reproducer: GtkWindow + WebKitWebView
 * Build: gcc -o 01-webkit 01-webkit.c $(pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.1)
 */
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

static gboolean quit_cb (gpointer data) { gtk_main_quit (); return G_SOURCE_REMOVE; }

int main (int argc, char **argv)
{
  gtk_init (&argc, &argv);

  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  GtkWidget *view = webkit_web_view_new ();
  gtk_container_add (GTK_CONTAINER (window), view);
  webkit_web_view_load_html (WEBKIT_WEB_VIEW (view), "<html><body>hello</body></html>", NULL);

  gtk_widget_show_all (window);
  g_timeout_add_seconds (5, quit_cb, NULL);
  gtk_main ();
  g_print ("survived\n");
  return 0;
}
