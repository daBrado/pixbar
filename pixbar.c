#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <poll.h>
#include <xcb/xcb.h>

#define BUF_SIZE 1024

#ifdef NDEBUG
  #define DOUT(TEXT)
  #define DVOUT(VAR,FMT)
  #define XCB(NAME, ...) xcb_ ## NAME (__VA_ARGS__)
#else
  #define DOUT(TEXT) fprintf(stderr, TEXT "\n")
  #define DVOUT(VAR,FMT) fprintf(stderr, #VAR " = %" FMT "\n", VAR)
  #define XCB(NAME, XCONN, ...) do { \
    xcb_void_cookie_t NAME ## _checked_cookie = xcb_ ## NAME ## _checked(XCONN, __VA_ARGS__); \
    xcb_generic_error_t* NAME ## _error; \
    if ((NAME ## _error = xcb_request_check(XCONN, NAME ## _checked_cookie))) { \
      fprintf(stderr, "ERROR " #NAME " = %d", NAME ## _error->error_code); \
      if (NAME ## _error->error_code == XCB_VALUE) fprintf(stderr, ", bad_value = %d", ((xcb_value_error_t*)NAME ## _error)->bad_value); \
      fprintf(stderr, "\n"); \
      free(NAME ## _error); \
    } \
  } while(0)
#endif

typedef enum { LEFT, RIGHT, TOP, BOTTOM } edge_t;

// Bad form since immeditately get reply after request, but let's assume we're only getting a few atoms.
xcb_atom_t get_atom(xcb_connection_t *xconn, const char* name)
{
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(xconn, 0, strlen(name), name);
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(xconn, cookie, NULL);
  if (!reply) exit(1);
  return reply->atom;
}

bool get_ul(char** start, char* end, bool hex, uint32_t* value)
{
  char* pos = *start;
  for(; pos!=end && (hex ? isxdigit(*pos) : isdigit(*pos)); pos++);
  if (pos==end) return false;
  char tmp = *pos;
  *pos = '\0';
  *value = strtoul(*start, NULL, hex ? 16 : 10);
  *pos = tmp;
  *start = pos;
  return true;
}

void set_color(xcb_connection_t* xconn, xcb_colormap_t colormap, xcb_gcontext_t gc, uint32_t color)
{
  uint16_t r = ((color&0xff0000)>>16)*0xffff/0xff;
  uint16_t g = ((color&0xff00)>>8)*0xffff/0xff;
  uint16_t b = (color&0xff)*0xffff/0xff;
  xcb_alloc_color_cookie_t cookie = xcb_alloc_color(xconn, colormap, r, g, b);
  xcb_alloc_color_reply_t *reply = xcb_alloc_color_reply(xconn, cookie, NULL);
  if (!reply) { fprintf(stderr, "could not allocate color\n"); return; }
  xcb_change_gc(xconn, gc, XCB_GC_FOREGROUND, (const uint32_t []){ reply->pixel });
  free(reply);
}

void draw_pixels(xcb_connection_t* xconn, xcb_pixmap_t pixmap, xcb_gcontext_t gc,
  edge_t edge, uint16_t width, uint16_t pos, uint16_t count)
{
  const xcb_rectangle_t rect = {
    (edge==TOP||edge==BOTTOM) ? pos : 0,
    (edge==TOP||edge==BOTTOM) ? 0 : pos,
    (edge==TOP||edge==BOTTOM) ? count : width,
    (edge==TOP||edge==BOTTOM) ? width : count
  };
  XCB(poly_fill_rectangle, xconn, pixmap, gc, 1, &rect);
}

int main(int argc, const char** argv)
{
  if (argc != 6) {
    fprintf(stderr, "usage: %s <x> <y> <width> <length> <edge>\n", argv[0]);
    exit(1);
  }
  uint16_t x = strtoul(argv[1], NULL, 10);
  uint16_t y = strtoul(argv[2], NULL, 10);
  uint16_t width = strtoul(argv[3], NULL, 10);
  uint16_t length = strtoul(argv[4], NULL, 10);
  edge_t edge;
  switch (argv[5][0]) {
    case 'l': edge = LEFT; break;
    case 'r': edge = RIGHT; break;
    case 't': edge = TOP; break;
    case 'b': edge = BOTTOM; break;
    default: fprintf(stderr, "edge must be one of: l r t b\n"); exit(1);
  }

  xcb_connection_t *xconn = xcb_connect(NULL, NULL);

  xcb_screen_t *screen;
  {
    const xcb_setup_t *setup = xcb_get_setup (xconn);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator (setup);
    screen = iter.data;
  }

  uint16_t win_x = (edge==RIGHT) ? (x-width) : x;
  uint16_t win_y = (edge==BOTTOM) ? (y-width) : y;
  uint16_t win_width = (edge==LEFT||edge==RIGHT) ? width : length;
  uint16_t win_height = (edge==LEFT||edge==RIGHT) ? length : width;
  uint16_t strut_size;
  switch (edge) {
    case   LEFT: strut_size = x + width; break;
    case  RIGHT: strut_size = screen->width_in_pixels - x + width; break;
    case    TOP: strut_size = y + width; break;
    case BOTTOM: strut_size = screen->height_in_pixels - y + width; break;
  }
  uint16_t strut_start = (edge==LEFT||edge==RIGHT) ? y : x;
  uint16_t strut_end = length;

  xcb_colormap_t colormap = xcb_generate_id(xconn);
  XCB(create_colormap, xconn, XCB_COLORMAP_ALLOC_NONE, colormap, screen->root, screen->root_visual);

  xcb_window_t window;
  {
    window = xcb_generate_id (xconn);
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
    uint32_t values[] = { screen->black_pixel, XCB_EVENT_MASK_EXPOSURE, colormap };
    XCB(create_window,
      xconn,
      XCB_COPY_FROM_PARENT,
      window,
      screen->root,
      win_x, win_y, win_width, win_height, 0 /*border*/,
      XCB_WINDOW_CLASS_INPUT_OUTPUT,
      screen->root_visual,
      mask, values
    );
  }

  {
    xcb_atom_t type = get_atom(xconn, "_NET_WM_WINDOW_TYPE");
    xcb_atom_t dock = get_atom(xconn, "_NET_WM_WINDOW_TYPE_DOCK");
    XCB(change_property, xconn, XCB_PROP_MODE_REPLACE, window, type, XCB_ATOM_ATOM, 32, 1, &dock);
  }
  {
    xcb_atom_t state = get_atom(xconn, "_NET_WM_STATE");
    xcb_atom_t sticky = get_atom(xconn, "_NET_WM_STATE_STICKY");
    xcb_atom_t above = get_atom(xconn, "_NET_WM_STATE_ABOVE");
    XCB(change_property, xconn, XCB_PROP_MODE_APPEND, window, state, XCB_ATOM_ATOM, 32, 1, &sticky);
    XCB(change_property, xconn, XCB_PROP_MODE_APPEND, window, state, XCB_ATOM_ATOM, 32, 1, &above);
  }
  {
    xcb_atom_t strut_partial = get_atom(xconn, "_NET_WM_STRUT_PARTIAL");
    xcb_atom_t strut = get_atom(xconn, "_NET_WM_STRUT");
    uint32_t struts[12] = {0};
    struts[edge] = strut_size;
    struts[4+edge*2] = strut_start;
    struts[4+edge*2+1] = strut_end;
    XCB(change_property, xconn, XCB_PROP_MODE_REPLACE, window, strut_partial, XCB_ATOM_CARDINAL, 32, 12, struts);
    XCB(change_property, xconn, XCB_PROP_MODE_REPLACE, window, strut, XCB_ATOM_CARDINAL, 32, 4, struts);
  }
  {
    xcb_atom_t desktop = get_atom(xconn, "_NET_WM_DESKTOP");
    const uint32_t ALL_DESKTOPS = -1;
    XCB(change_property, xconn, XCB_PROP_MODE_REPLACE, window, desktop, XCB_ATOM_CARDINAL, 32, 1, &ALL_DESKTOPS);
  }

  xcb_pixmap_t pixmap = xcb_generate_id(xconn);
  XCB(create_pixmap, xconn, screen->root_depth, pixmap, window, win_width, win_height);

  xcb_gcontext_t draw_gc = xcb_generate_id(xconn);
  XCB(create_gc, xconn, draw_gc, pixmap, XCB_GC_FOREGROUND | XCB_GC_BACKGROUND, (const uint32_t []){ screen->black_pixel, screen->black_pixel });

  XCB(poly_fill_rectangle, xconn, pixmap, draw_gc, 1, (const xcb_rectangle_t []){ { 0, 0, win_width, win_height } });

  XCB(map_window, xconn, window);
  xcb_flush(xconn);

  struct pollfd pollfds[] = {
    { .fd = STDIN_FILENO, .events = POLLIN },
    { .fd = xcb_get_file_descriptor(xconn), .events = POLLIN },
  };
  enum { POLLFD_STDIN, POLLFD_X, POLLFD_NUM };
  enum {CMD, COLOR, POS, DRAW} readmode = CMD;
  char buf[BUF_SIZE];
  char* buf_start = buf;
  char* buf_end = buf;
  uint32_t pix_pos = 0;
  bool done = false;
  while (!done) {
    bool redraw = false;
    xcb_generic_event_t *event;
    if (poll(pollfds, POLLFD_NUM, -1 /*timeout*/) > 0) {
      if (pollfds[POLLFD_STDIN].revents & POLLHUP) done = true;
      if (pollfds[POLLFD_STDIN].revents & POLLIN) {
        ssize_t bytes = read(STDIN_FILENO, buf_end, BUF_SIZE-(buf_end-buf));
        if (bytes <= 0) done = true; else buf_end += bytes;
      }
      if (pollfds[POLLFD_X].revents & POLLIN) {
        while ((event = xcb_poll_for_event(xconn))) {
          if ((event->response_type & ~0x80) == XCB_EXPOSE) redraw = true;
          free(event);
        }
      }
    }
    for (char* prev_start=0; buf_start!=buf_end && buf_start!=prev_start;) {
      prev_start = buf_start;
      switch (readmode) {
      case CMD:
        switch (*buf_start) {
        case '#':
          readmode = COLOR; DOUT("color mode"); break;
        case '@':
          readmode = POS; DOUT("position mode"); break;
        case '*':
          readmode = DRAW; DOUT("draw mode"); break;
        case '\n':
          redraw = true; DOUT("request redraw"); break;
        default:
          fprintf(stderr, "unknown command %c\n", *buf_start); break;
        }
        buf_start++;
        break;
      case COLOR: ;
        uint32_t color;
        if (!get_ul(&buf_start, buf_end, true /*hex*/, &color)) break; DVOUT(color,"06x");
        readmode = CMD;
        set_color(xconn, colormap, draw_gc, color);
        break;
      case POS:
        if (!get_ul(&buf_start, buf_end, false /*hex*/, &pix_pos)) break; DVOUT(pix_pos,"d");
        readmode = CMD;
        break;
      case DRAW: ;
        uint32_t count;
        if (!get_ul(&buf_start, buf_end, false /*hex*/, &count)) break; DVOUT(count,"d");
        readmode = CMD;
        draw_pixels(xconn, pixmap, draw_gc, edge, width, pix_pos, count);
        break;
      }
      if (readmode==CMD && buf_start==buf_end) { buf_start=buf_end=buf; DOUT("reset buf"); }
    }
    if (redraw) { XCB(copy_area, xconn, pixmap, window, draw_gc, 0, 0, 0, 0, win_width, win_height); DOUT("draw pixmap"); }
    xcb_flush(xconn);
  }

  XCB(free_gc, xconn, draw_gc);
  XCB(free_pixmap, xconn, pixmap);
  XCB(free_colormap, xconn, colormap);
  xcb_disconnect(xconn);
  return 0;
}
