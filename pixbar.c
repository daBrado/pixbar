#include <ctype.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

// Some code learned from http://vincentsanders.blogspot.com/2010/04/xcb-programming-is-hard.html

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
  xcb_image_t *image;
  {
    const xcb_setup_t *setup = xcb_get_setup(xconn);
    { // Get an X screen.
      xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
      screen = iter.data;
    }
    { // Get an X pixmap image with 24 bit depth and 32 bits per pixel.
      xcb_format_t *fmt = xcb_setup_pixmap_formats(setup);
      xcb_format_t *fmtend = fmt + xcb_setup_pixmap_formats_length(setup);
      for (; fmt != fmtend; ++fmt) {
        if((fmt->depth == 24) && (fmt->bits_per_pixel == 32)) {
          break;
        }
      }
      if (fmt == fmtend) {
        fprintf(stderr, "could not find a suitable pixmap format\n"); exit(1);
      }
      unsigned char *image_data = (unsigned char *)malloc(length*4);
      for (unsigned char *i = image_data; (i-image_data) < length*4; ++i) {
        *i = 0;
      }
      image = xcb_image_create(
        (edge==LEFT)||(edge==RIGHT) ? 1 : length,
        (edge==LEFT)||(edge==RIGHT) ? length : 1,
        XCB_IMAGE_FORMAT_Z_PIXMAP, fmt->scanline_pad, fmt->depth, fmt->bits_per_pixel, 0,
        setup->image_byte_order, XCB_IMAGE_ORDER_LSB_FIRST, image_data, length*4, image_data);
      if (image == NULL) {
        fprintf(stderr, "could not create image\n"); exit(1);
      }
    }
  }

  xcb_colormap_t colormap = xcb_generate_id(xconn);
  XCB(create_colormap, xconn, XCB_COLORMAP_ALLOC_NONE, colormap, screen->root, screen->root_visual);

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

  xcb_window_t window;
  {
    window = xcb_generate_id(xconn);
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

  xcb_image_put(xconn, pixmap, draw_gc, image, 0, 0, 0);

  XCB(map_window, xconn, window);
  xcb_flush(xconn);

  struct pollfd pollfds[] = {
    { .fd = STDIN_FILENO, .events = POLLIN },
    { .fd = xcb_get_file_descriptor(xconn), .events = POLLIN },
  };
  enum { POLLFD_STDIN, POLLFD_X, POLLFD_NUM };

  char* buf = malloc(length*3);
  char* buf_pos = buf;
  char* buf_end = buf+(length*3);
  bool done = false;
  while (!done) {
    bool redraw = false;
    xcb_generic_event_t *event;
    if (poll(pollfds, POLLFD_NUM, -1 /*timeout*/) > 0) {
      if (pollfds[POLLFD_STDIN].revents & POLLHUP) done = true;
      if (pollfds[POLLFD_STDIN].revents & POLLIN) {
        ssize_t bytes = read(STDIN_FILENO, buf_pos, buf_end-buf_pos);
        if (bytes <= 0) {
          done = true;
        } else {
          for (; buf_pos!=buf+bytes; ++buf_pos) {
            image->data[4*(buf_pos-buf)/3] = *buf_pos;
          }
          if (buf_pos == buf_end) {
            for (uint16_t w = 0; w < width; ++w) {
              xcb_image_put(xconn, pixmap, draw_gc, image,
                (edge==LEFT)||(edge==RIGHT) ? w : 0,
                (edge==LEFT)||(edge==RIGHT) ? 0 : w,
                0);
            }
            buf_pos = buf;
            redraw = true;
          }
        }
      }
      if (pollfds[POLLFD_X].revents & POLLIN) {
        while ((event = xcb_poll_for_event(xconn))) {
          if ((event->response_type & ~0x80) == XCB_EXPOSE) redraw = true;
          free(event);
        }
      }
    }
    if (redraw) {
      XCB(copy_area, xconn, pixmap, window, draw_gc, 0, 0, 0, 0, win_width, win_height); DOUT("draw pixmap");
    }
    xcb_flush(xconn);
  }

  XCB(free_gc, xconn, draw_gc);
  XCB(free_pixmap, xconn, pixmap);
  XCB(free_colormap, xconn, colormap);
  xcb_disconnect(xconn);
  free(buf);
  return 0;
}
