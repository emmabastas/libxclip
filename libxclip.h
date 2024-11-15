#ifndef LIBXCLIP_H_
#define LIBXCLIP_H_
#include <unistd.h>
#include <X11/Xlib.h>
typedef struct PutOptions PutOptions;
int libxclip_put(Display *display, char *data, size_t len, PutOptions *options);
#endif  // LIBXCLIP_H_

