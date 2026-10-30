// This module implements the Z-100 screen using the gtk graphics library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "screen.h"
#include "mainboard.h"

// get access to external types define elsewhere
// **DO WE NEED THESE?? SHOULD THEY NOT COME FROM THE MAINBOARD DEFINITIONS via
// A HEADER FILE?
extern unsigned int* pixels;
Z100* z100;

GtkWidget *window;
GtkWidget *drawingArea;
static GtkWidget *menuBar;
static int pixelHeight = 2;
static GMutex frameMutex;
static unsigned int frame[VWIDTH * VHEIGHT];
static volatile gint redrawPending;

int screenParseOptions(int* argc, char*** argv)
{
	for(int i = 1; i < *argc; i++) {
		if(!strncmp((*argv)[i], "--pixels=", 9)) {
			const char* value = (*argv)[i] + 9;
			if(!strcmp(value, "1x1")) pixelHeight = 1;
			else if(!strcmp(value, "1x2")) pixelHeight = 2;
			else { fprintf(stderr, "Use --pixels=1x1 or --pixels=1x2\n"); return 0; }
			for(int j = i; j < *argc; j++) (*argv)[j] = (*argv)[j + 1];
			(*argc)--; i--;
		}
	}
	return 1;
}

static gboolean queueFrame(gpointer unused)
{
	(void)unused;
	g_atomic_int_set(&redrawPending, 0);
	if(drawingArea) gtk_widget_queue_draw(drawingArea);
	return G_SOURCE_REMOVE;
}

/* Called by the emulator thread after renderScreen. Publish a frame, then
   ask GTK's main thread to redraw it. GTK never reads live VRAM. */
void display()
{
	if(!pixels) return;
	g_mutex_lock(&frameMutex);
	memcpy(frame, pixels, sizeof(frame));
	g_mutex_unlock(&frameMutex);
	if(g_atomic_int_compare_and_exchange(&redrawPending, 0, 1))
		g_idle_add(queueFrame, NULL);
}

static void resizePixelCanvas(void)
{
	int scale = gtk_widget_get_scale_factor(drawingArea);
	if(scale < 1) scale = 1;
	int width = (VWIDTH + scale - 1) / scale;
	int height = (VHEIGHT * pixelHeight + scale - 1) / scale;
	int menuHeight = 0;
	gtk_widget_get_preferred_height(menuBar, NULL, &menuHeight);
	gtk_widget_set_size_request(drawingArea, width, height);
	gtk_window_resize(GTK_WINDOW(window), width, height + menuHeight);
	gtk_window_set_title(GTK_WINDOW(window), pixelHeight == 2 ?
		"Z-100 Screen - pixels 1x2" : "Z-100 Screen - pixels 1x1");
	gtk_widget_queue_draw(drawingArea);
}

static void pixelModeChanged(GtkCheckMenuItem* item, gpointer height)
{
	if(!gtk_check_menu_item_get_active(item)) return;
	pixelHeight = GPOINTER_TO_INT(height);
	resizePixelCanvas();
}

void windowactive()
{
	GdkWindow* w=gtk_widget_get_window(window);
	gdk_window_show(w);
	gdk_window_focus(w,0);
//	w->gtk_window_present_with_time();
#ifdef RPI
	gtk_window_fullscreen(GTK_WINDOW(window));
#endif
}

void windowinactive()
{
	GdkWindow* w=gtk_widget_get_window(window);
	gdk_window_hide(w);
}

static gboolean on_keypress(GtkWidget* widget, GdkEventKey* event) {
  // the 'return' char will be used for any incoming keycode that does not
  // correspond to a standard ascii digit or letter - (set the default key code
  // to '\r')
  if(!z100 || !z100->keyboard) return FALSE;
  char code = 0x00;
  /* the GdkEventKey* event is a keyboard hardware event that is detected
    from the user machine. What gets used here is the event->keyval instance
    variable of the GdkEventKey object.
    "keyval" is either one of two things - an ascii character or a keycode */
  printf("*screen: key pressed* GdkEventKey Code: %x\n", event->keyval);

  //handle CONTROL key press
  if(event->keyval == 0xffe3)
  {
    code=0x03;
  }

  // handle BACKSPACE key press
  if(event->keyval == 0xff08) {
    code = 0x08;
  }
  // handle TAB key press
  else if(event->keyval == 0xff09) {
    code = 0x09;
  }
  // handle RETURN key press
  else if(event->keyval == 0xff0d) {
    code = 0x0d;
  }
  // handle ESC key press
  else if(event->keyval == 0xff1b) {
    code = 0x1b;
  }
  // handle DEL key press
  else if(event->keyval == 0xffff) {
    code = 0x7f;
  }
  // handle ENTER (keypad) key press
  else if(event->keyval == 0xff8d) {
    code = (char)0x8d;
  }
  // F0 NOT implemented
//F1-F12
else if (event->keyval == 0xffbe) {
	code=(char)0x97;
}
else if (event->keyval == 0xffbf) {
	code=(char)0x98;
}
else if (event->keyval == 0xffc0) {
	code=(char)0x99;
}
else if (event->keyval == 0xffc1) {
	code=(char)0x9a;
}
else if (event->keyval == 0xffc2) {
	code=(char)0x9b;
}
else if (event->keyval == 0xffc3) {
	code=(char)0x9c;
}
else if (event->keyval == 0xffc4) {
	code=(char)0x9d;
}
else if (event->keyval == 0xffc5) {
	code=(char)0x9e;
}
else if (event->keyval == 0xffc6) {
	code=(char)0x9f;
}
else if (event->keyval == 0xffc7) {
	code=(char)0xa0;
}
else if (event->keyval == 0xffc8) {
	code=(char)0xa1;
}
else if (event->keyval == 0xffc9) {
	code=(char)0xa2;
}
  // handle UP key press
  else if(event->keyval == 0xff52) {
    code = (char)0xa5;
//substitute ctrl-k
//code=(char)0xb;
  }
  // handle DOWN key press
  else if(event->keyval == 0xff54) {
    code = (char)0xa6;
//substitute ctrl-j
//code=(char)0xa;
  }
  // handle RIGHT key press
  else if(event->keyval == 0xff53) {
    code = (char)0xa7;
//substitute ctrl-l
//code=(char)0xc;
  }
  // handle LEFT key press
  else if(event->keyval == 0xff51) {
    code = (char)0xa8;
//substitute ctrl-h
//code=(char)0x8;
  }
  // handle HOME (numLock off-keypad 7 (Home)) key press
  else if(event->keyval == 0xff95) {
    code = (char)0xa9;
  }
  // handle BREAK key press
  else if(event->keyval == 0xff13) {
    code = (char)0xaa;
  }
  else if(event->keyval == 0xff57) {
	togglePause();
  }
  // handle keypad '-' '.' key press
  else if(event->keyval == 0xffad || event->keyval == 0xffae ||
    (event->keyval >= 0xffb0 && event->keyval <= 0xffb9)) {
    code = event->keyval & 0xFF;
  }
  // handle all other keys where event->keyval = z-100 keycodes
  else if(event->keyval >= 0x20 && event->keyval <= 0x7E) {
    code = event->keyval;
  }
  // call function keyaction in keyboard.c, which loads the keyboard buffer
  // with the pressed key code
  if(code!=0)
	  keyaction(z100->keyboard, code);
  return code != 0;
}

/* Draw in physical device pixels, even on a GTK HiDPI surface. Window
   resizing adds margins; it never applies fractional scaling to the image. */
static void paintPixels(cairo_t* cr, int width, int height, int yscale, unsigned int* data)
{
	double deviceX = 1, deviceY = 1;
	cairo_surface_get_device_scale(cairo_get_target(cr), &deviceX, &deviceY);
	cairo_save(cr);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_paint(cr);
	cairo_scale(cr, 1.0 / deviceX, 1.0 / deviceY);
	int x = ((int)(width * deviceX) - VWIDTH) / 2;
	int y = ((int)(height * deviceY) - VHEIGHT * yscale) / 2;
	if(x < 0) x = 0;
	if(y < 0) y = 0;
	cairo_translate(cr, x, y);
	cairo_scale(cr, 1, yscale);
	cairo_surface_t* image = cairo_image_surface_create_for_data((unsigned char*)data,
		CAIRO_FORMAT_RGB24, VWIDTH, VHEIGHT, VWIDTH * sizeof(*data));
	cairo_set_source_surface(cr, image, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	cairo_rectangle(cr, 0, 0, VWIDTH, VHEIGHT);
	cairo_fill(cr);
	cairo_surface_destroy(image);
	cairo_restore(cr);
}

static gboolean on_draw_event(GtkWidget* widget, cairo_t* cr)
{
	g_mutex_lock(&frameMutex);
	paintPixels(cr, gtk_widget_get_allocated_width(widget),
		gtk_widget_get_allocated_height(widget), pixelHeight, frame);
	g_mutex_unlock(&frameMutex);
	return FALSE;
}

void screenInit(int* argc, char** argv[])
{
	gtk_init(argc, argv);
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	drawingArea = gtk_drawing_area_new();
	GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(window), box);
	menuBar = gtk_menu_bar_new();
	GtkWidget* view = gtk_menu_item_new_with_label("View");
	GtkWidget* menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(view), menu);
	gtk_menu_shell_append(GTK_MENU_SHELL(menuBar), view);
	GtkWidget* square = gtk_radio_menu_item_new_with_label(NULL, "Pixels 1x1 (point for point)");
	GtkWidget* tall = gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(square), "Pixels 1x2 (double height)");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), square);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), tall);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pixelHeight == 2 ? tall : square), TRUE);
	gtk_box_pack_start(GTK_BOX(box), menuBar, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), drawingArea, TRUE, TRUE, 0);
	g_signal_connect(square, "toggled", G_CALLBACK(pixelModeChanged), GINT_TO_POINTER(1));
	g_signal_connect(tall, "toggled", G_CALLBACK(pixelModeChanged), GINT_TO_POINTER(2));
	g_signal_connect(drawingArea, "draw", G_CALLBACK(on_draw_event), NULL);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(window, "key-press-event", G_CALLBACK(on_keypress), NULL);
	resizePixelCanvas();
	gtk_widget_show_all(window);
#ifdef RPI
	gtk_window_fullscreen(GTK_WINDOW(window));
#endif
}

void screenSetComputer(Z100* c)
{
	z100=c;
}

void screenLoop() {
  // keep GTK window open - will not return
  gtk_main();
}
