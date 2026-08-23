/* Control: same window lifecycle, cairo only, no GL, no WebKit.
 * Build: gcc -o 03-plain 03-plain.c $(pkg-config --cflags --libs gtk+-3.0)
 */
#include <gtk/gtk.h>

static gboolean quit_cb (gpointer data) { gtk_main_quit (); return G_SOURCE_REMOVE; }

int main (int argc, char **argv)
{
  gtk_init (&argc, &argv);

  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_container_add (GTK_CONTAINER (window), gtk_label_new ("hello"));

  gtk_widget_show_all (window);
  g_timeout_add_seconds (5, quit_cb, NULL);
  gtk_main ();
  g_print ("survived\n");
  return 0;
}
