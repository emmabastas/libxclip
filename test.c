#include "libxclip.h"

#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

Display *display;

Atom a_clipboard;

int main(void) {
    display = XOpenDisplay(NULL);

    a_clipboard = XInternAtom(display, "CLIPBOARD", False);

    XSetSelectionOwner(display, a_clipboard, None, CurrentTime);

    char *data = "Hello! :-)";
    libxclip_put(display, data, strlen(data));
    return 0;
}
