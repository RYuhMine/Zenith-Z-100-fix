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

/* Remember the identity seen on key-down: releasing Shift can change the
   keyval reported for a held key, but its Z-100 key-up code must stay paired. */
static unsigned int heldKeys[256];

static unsigned int keyboardCode(guint keyval)
{
    if(keyval >= GDK_KEY_F1 && keyval <= GDK_KEY_F12) return 0x97 + keyval - GDK_KEY_F1;
    if(keyval >= GDK_KEY_KP_0 && keyval <= GDK_KEY_KP_9) return 0xb0 + keyval - GDK_KEY_KP_0;
    if(keyval >= 0x20 && keyval <= 0x7e) return keyval;
    switch(keyval) {
    case GDK_KEY_Shift_L: return KEY_SHIFT_LEFT;
    case GDK_KEY_Shift_R: return KEY_SHIFT_RIGHT;
    case GDK_KEY_Control_L: case GDK_KEY_Control_R: return KEY_CONTROL;
    case GDK_KEY_Caps_Lock: return KEY_CAPS_LOCK;
    case GDK_KEY_Alt_L: return KEY_FAST_REPEAT;
    case GDK_KEY_Alt_R: return 0x0a;
    case GDK_KEY_BackSpace: return 8;
    case GDK_KEY_Tab: case GDK_KEY_ISO_Left_Tab: return 9;
    case GDK_KEY_Return: return 13;
    case GDK_KEY_Escape: return 0x1b;
    case GDK_KEY_Delete: return 0x7f;
    case GDK_KEY_KP_Enter: return 0x8d;
    case GDK_KEY_Help: return 0x95;
    case GDK_KEY_F13: return 0xa4;
    case GDK_KEY_F14: return 0xa3;
    case GDK_KEY_Up: return 0xa5;
    case GDK_KEY_Down: return 0xa6;
    case GDK_KEY_Right: return 0xa7;
    case GDK_KEY_Left: return 0xa8;
    case GDK_KEY_Home: return 0xa9;
    case GDK_KEY_Pause: case GDK_KEY_Break: return 0xaa;
    case GDK_KEY_KP_Subtract: return 0xad;
    case GDK_KEY_KP_Decimal: case GDK_KEY_KP_Delete: return 0xae;
    case GDK_KEY_KP_Insert: return 0xb0;
    case GDK_KEY_KP_End: return 0xb1;
    case GDK_KEY_KP_Down: return 0xb2;
    case GDK_KEY_KP_Page_Down: return 0xb3;
    case GDK_KEY_KP_Left: return 0xb4;
    case GDK_KEY_KP_Begin: return 0xb5;
    case GDK_KEY_KP_Right: return 0xb6;
    case GDK_KEY_KP_Home: return 0xb7;
    case GDK_KEY_KP_Up: return 0xb8;
    case GDK_KEY_KP_Page_Up: return 0xb9;
    }
    return 0;
}

static gboolean on_keypress(GtkWidget* widget, GdkEventKey* event)
{
    (void)widget;
    if(!z100 || !z100->keyboard) return FALSE;
    if(event->keyval == GDK_KEY_End) { togglePause(); return TRUE; }
    unsigned int code = keyboardCode(event->keyval);
    if(event->hardware_keycode < G_N_ELEMENTS(heldKeys)) {
        unsigned int* held = &heldKeys[event->hardware_keycode];
        if(!*held) *held = code;
    }
    if(code) keyboardKeyEvent(z100->keyboard, code, 1);
    return code != 0;
}

static gboolean on_keyrelease(GtkWidget* widget, GdkEventKey* event)
{
    (void)widget;
    if(!z100 || !z100->keyboard) return FALSE;
    unsigned int code = keyboardCode(event->keyval);
    if(event->hardware_keycode < G_N_ELEMENTS(heldKeys)) {
        unsigned int* held = &heldKeys[event->hardware_keycode];
        if(*held) code = *held;
        *held = 0;
    }
    if(code) keyboardKeyEvent(z100->keyboard, code, 0);
    return code != 0;
}

static gboolean on_focus_out(GtkWidget* widget, GdkEventFocus* event)
{
    (void)widget; (void)event;
    for(unsigned int i = 0; i < G_N_ELEMENTS(heldKeys); i++) {
        if(heldKeys[i] && z100 && z100->keyboard) keyboardKeyEvent(z100->keyboard, heldKeys[i], 0);
        heldKeys[i] = 0;
    }
    return FALSE;
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
	g_signal_connect(window, "key-release-event", G_CALLBACK(on_keyrelease), NULL);
	g_signal_connect(window, "focus-out-event", G_CALLBACK(on_focus_out), NULL);
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
