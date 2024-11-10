#ifndef LIBXCLIP_H_INCLUDED
#define LIBXCLIP_H_INCLUDED
#include <unistd.h>
#include <X11/Xlib.h>
pid_t libxclip_put(Display *display, char *data, size_t len);
#endif

