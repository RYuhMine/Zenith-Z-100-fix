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

//void renderScreen(Video*, unsigned int*);

#ifdef RPI
const float X_SCALE = 0.75;
const float Y_SCALE = 1.50;
#endif
#ifndef RPI
const float X_SCALE = 1;
const float Y_SCALE = 2;
#endif
GtkWidget *window;
GtkWidget *drawingArea;

// gtk function to draw in gtk window
void display() {
  gtk_widget_queue_draw(window);
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
}


/* -------------------------------------------------------------------------
   Scripted keyboard, for unattended runs and regression testing.

   Set the Z100_KEYS environment variable to the path of a script file and the
   emulator will drive itself. One action per line:

       # comment
       wait 5          wait five seconds
       settle 3        wait until the screen has been unchanged for 3 seconds
       text dir        type that string, one key every 200 ms
       key 0d          send a raw key code in hex (0d is RETURN)
       disk b work.img swap the image in drive A or B while running

   The file is re-read while the emulator runs, so actions can be appended to
   a live session.

   Why this exists: on a rootless X server such as WSLg there is no window
   manager, input focus does not stick, and xdotool cannot reach the GTK key
   handler. Without this there is no reliable way to boot the machine
   unattended or to reproduce a bug from a fixed sequence of keystrokes.
   -------------------------------------------------------------------------*/
extern char* image_name_a;
extern char* image_name_b;
void reloadDisk(JWD1797*);

enum { ACT_WAIT, ACT_TEXT, ACT_KEY, ACT_DISK, ACT_SETTLE };

typedef struct { int kind; int value; char* text; } KeyAction;

#define KEY_ACTIONS_MAX 4096
#define KEY_TICK_MS     50
#define KEY_TYPE_TICKS  4      /* one keystroke every 4 ticks, 200 ms */

static KeyAction* kact = NULL;
static int kact_n = 0, kact_i = 0, kact_ci = 0;
static int kact_wait = 0, kact_tick = 0, kact_idle = 0;
static unsigned int kact_sum = 0;
static int kact_stable = 0, kact_limit = 0;
static long kact_pos = 0;
static char kact_path[512];

static int keyScriptRead(void);

/* Fingerprint of video memory, sampled one word in sixteen. Used to tell when
   the machine has finished drawing. */
static unsigned int vramSum(void) {
  unsigned int s = 0;
  const unsigned int* v = z100->video->vram;
  for (int i = 0; i < 0x10000 * 3; i += 16) s = s * 31u + v[i];
  return s;
}

static gboolean keyScriptStep(gpointer data) {
  if (z100 == NULL || z100->keyboard == NULL) return TRUE;   /* still booting */

  if (kact_i >= kact_n) {
    /* Out of actions: poll the file once a second so a running session can be
       extended by appending to it. */
    if (++kact_idle >= 1000 / KEY_TICK_MS) { kact_idle = 0; keyScriptRead(); }
    return TRUE;
  }

  KeyAction* a = &kact[kact_i];

  if (kact_wait > 0) { kact_wait--; return TRUE; }

  switch (a->kind) {
    case ACT_WAIT:
      kact_wait = a->value; kact_i++; return TRUE;

    case ACT_SETTLE: {
      unsigned int s = vramSum();
      if (s == kact_sum) kact_stable++;
      else { kact_sum = s; kact_stable = 0; }
      kact_limit++;
      /* The cursor blinks, so on some screens the image never becomes
         completely stable. Give up waiting rather than stall forever. */
      if (kact_stable >= a->value || kact_limit >= a->value * 3 + 400) {
        kact_stable = 0; kact_limit = 0; kact_i++;
      }
      return TRUE;
    }

    case ACT_KEY:
      keyaction(z100->keyboard, a->value); kact_i++; return TRUE;

    case ACT_DISK: {
      char** slot = (a->value == 0) ? &image_name_a : &image_name_b;
      *slot = (char*)malloc(strlen(a->text) + 1);
      strcpy(*slot, a->text);
      reloadDisk(z100->jwd1797);
      printf("key script: drive %c now holds %s\n", a->value ? 'B' : 'A', a->text);
      kact_i++; return TRUE;
    }

    case ACT_TEXT:
      if (a->text[kact_ci] == '\0') { kact_ci = 0; kact_i++; return TRUE; }
      /* The keyboard FIFO is 17 bytes deep and keystrokes are dropped if it
         overruns while the drive is busy, so type at a human rate. */
      if (++kact_tick < KEY_TYPE_TICKS) return TRUE;
      kact_tick = 0;
      keyaction(z100->keyboard, (unsigned char)a->text[kact_ci++]);
      return TRUE;
  }
  kact_i++;
  return TRUE;
}

static void keyScriptAdd(int kind, int value, const char* text) {
  if (kact_n >= KEY_ACTIONS_MAX) return;
  kact[kact_n].kind = kind;
  kact[kact_n].value = value;
  kact[kact_n].text = text ? strdup(text) : NULL;
  kact_n++;
}

static int keyScriptRead(void) {
  FILE* fp = fopen(kact_path, "r");
  if (fp == NULL) return 0;
  fseek(fp, kact_pos, SEEK_SET);
  int before = kact_n;
  char line[512];
  while (fgets(line, sizeof(line), fp) != NULL) {
    char* p = line;
    while (*p == ' ' || *p == '\t') p++;
    int n = strlen(p);
    while (n > 0 && (p[n - 1] == '\n' || p[n - 1] == '\r')) p[--n] = '\0';
    if (*p == '#' || *p == '\0') continue;
    if      (!strncmp(p, "wait ", 5))   keyScriptAdd(ACT_WAIT, atoi(p + 5) * (1000 / KEY_TICK_MS), NULL);
    else if (!strncmp(p, "settle ", 7)) keyScriptAdd(ACT_SETTLE, atoi(p + 7) * (1000 / KEY_TICK_MS), NULL);
    else if (!strncmp(p, "key ", 4))    keyScriptAdd(ACT_KEY, (int)strtol(p + 4, NULL, 16), NULL);
    else if (!strncmp(p, "text ", 5))   keyScriptAdd(ACT_TEXT, 0, p + 5);
    else if (!strncmp(p, "disk ", 5))   keyScriptAdd(ACT_DISK, (p[5] == 'b' || p[5] == 'B'), p + 7);
  }
  kact_pos = ftell(fp);
  fclose(fp);
  if (kact_n > before) printf("key script: %d new action(s), %d total\n", kact_n - before, kact_n);
  return kact_n - before;
}

static void keyScriptInit(void) {
  const char* f = getenv("Z100_KEYS");
  if (f == NULL) return;
  snprintf(kact_path, sizeof(kact_path), "%s", f);
  kact = (KeyAction*)malloc(sizeof(KeyAction) * KEY_ACTIONS_MAX);
  if (kact == NULL) return;
  keyScriptRead();
  printf("key script: watching %s\n", kact_path);
  g_timeout_add(KEY_TICK_MS, keyScriptStep, NULL);
}

static gboolean on_draw_event(GtkWidget* widget, cairo_t *cairo_obj) {
  if (pixels == NULL || z100 == NULL || z100->video == NULL) {
    return FALSE;
  }
  // holds 24-bit color from pixel array element
  unsigned int p24BitColor;
  // chars to hold each RGB color value
  unsigned char red_val;
  unsigned char green_val;
  unsigned char blue_val;
  /* populate the pixel array using the render screen function defined in video.c.
    This function reads the VRAM and sets up the pixel array accordingly */
  renderScreen(z100->video,pixels);
  // now, cycle through the pixel array as if reading rows/columns
  // loop through rows

if(X_SCALE!=(int)X_SCALE || Y_SCALE!=(int)Y_SCALE)
{
//https://stackoverflow.com/questions/9570895/image-downscaling-algorithm
int xwidth=(int)(X_SCALE*640);
int ywidth=(int)(Y_SCALE*225);
double yend=0.0;

  for(int row = 0; row < ywidth; row++) {
	double ystart=yend;
	yend=(row+1)/Y_SCALE;
	if(yend>=225)
		yend=225-.0001;
	double xend=0.0;

    // loop through columns
    for(int column = 0; column < xwidth; column++) {
	double xstart=xend;
	xend=(column+1)/X_SCALE;
	if(xend>=640)
		xend=640-.0001;
	double ravg=0.0,gavg=0.0,bavg=0.0;
	for(int y=(int)ystart; y<=(int)yend; ++y)
	{
		double yportion=1.0;
		if(y==(int)ystart)
			yportion-=ystart-y;
		if(y==(int)yend)
			yportion-=y+1-yend;
		for(int x=(int)xstart; x<=(int)xend; ++x)
		{
			double xportion=1.0;
			if(x==(int)xstart) xportion-=xstart-x;
			if(x==(int)xend) xportion-=x+1-xend;

      // get 24-bit colour from pixel array element
      p24BitColor = pixels[(y*640) + x];
      // extract each color component from the 24-bit color
      red_val = (p24BitColor>>16)&0xff;
      green_val = (p24BitColor>>8)&0xff;
      blue_val = p24BitColor&0xff;
	ravg+=red_val*yportion*xportion;
	gavg+=green_val*yportion*xportion;
	bavg+=blue_val*yportion*xportion;
		}
	}
	ravg=ravg/(X_SCALE*Y_SCALE);
	gavg=gavg/(X_SCALE*Y_SCALE);
	bavg=bavg/(X_SCALE*Y_SCALE);
	if(ravg>=256.0)ravg=255.0;
	if(gavg>=256.0){gavg=255.0; printf("Oops g=%f\n",gavg); }
	if(bavg>=256.0)bavg=255.0;
	red_val=(int)ravg;
	green_val=(int)gavg;
	blue_val=(int)bavg;
      // source RGB data to cairo rectangle
      cairo_set_source_rgb(cairo_obj, red_val, green_val, blue_val);
      // make rectangle (obj, x-coor of left side, y-coor of top, width, height)
      cairo_rectangle(cairo_obj, column, row, 1, 1);
      // color rectangle
      cairo_fill(cairo_obj);
    }
  }
}
else
{

  for(int row = 0; row < 225; row++) {
    // loop through columns
    for(int column = 0; column < 640; column++) {
      // get 24-bit colour from pixel array element
      p24BitColor = pixels[(row*640) + column];
      // extract each color component from the 24-bit color
      red_val = (p24BitColor>>16)&0xff;
      green_val = (p24BitColor>>8)&0xff;
      blue_val = p24BitColor&0xff;
      // source RGB data to cairo rectangle
      cairo_set_source_rgb(cairo_obj, red_val, green_val, blue_val);
      // make rectangle (obj, x-coor of left side, y-coor of top, width, height)
      int xsize=(int)X_SCALE;
      if (xsize<1) xsize=1;
      int ysize=(int)Y_SCALE;
      if (ysize<1) ysize=1;
      cairo_rectangle(cairo_obj, (int)(column*X_SCALE), (int)(row*Y_SCALE), xsize, ysize);
      // color rectangle
      cairo_fill(cairo_obj);
    }
  }
}
  return FALSE;
}

void screenInit(int* argc, char** argv[]) {

  gtk_init(argc, argv);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  drawingArea = gtk_drawing_area_new();
  // add drawing area to window
  gtk_container_add(GTK_CONTAINER(window), drawingArea);

  gtk_window_set_title(GTK_WINDOW(window), "Z-100 Screen");
  gtk_window_set_default_size(GTK_WINDOW(window), (int)(640*X_SCALE), (int)(225*Y_SCALE));

#ifdef RPI
  gtk_window_fullscreen(GTK_WINDOW(window));
#endif

  // connect callback functions to GTK window and drawing area default operations
  /* i.e. - when gtk_widget_queue_draw() is called via the display() function called
    from the main Z-100 loop in mainBoard.c, it qualifies as a "draw" event */
  g_signal_connect(G_OBJECT(drawingArea), "draw", G_CALLBACK(on_draw_event), NULL);
  g_signal_connect(G_OBJECT(drawingArea), "destroy", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(G_OBJECT(window), "key-press-event", G_CALLBACK(on_keypress), NULL);

  gtk_widget_show_all(window);
  keyScriptInit();
}

void screenSetComputer(Z100* c)
{
	z100=c;
}

void screenLoop() {
  // keep GTK window open - will not return
  gtk_main();
}
