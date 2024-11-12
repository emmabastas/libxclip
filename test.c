#include "libxclip.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

Display *display;

char *data;

Atom a_clipboard;

int main(void) {
    display = XOpenDisplay(NULL);

    a_clipboard = XInternAtom(display, "CLIPBOARD", False);

    printf("Clear the selection owner and verify it's not owned.\n");
    XSetSelectionOwner(display, a_clipboard, None, CurrentTime);
    XSync(display, False);
    printf("> xclip -o -selection clipboard: ");
    fflush(stdout);
    system("xclip -o -selection clipboard");

    printf("\n\n=== libxclip_put doesn't cause double printing\n");
    // See the comments next to __fpurge in libxclip.c for an explaination of
    // this test
    printf("libxclip_put prints: ");
    fflush(stdout);
    fwrite("I'm in the stdout buffer", 1, 24, stdout);
    libxclip_put(display, "", 0);
    fflush(stdout);
    XSetSelectionOwner(display, a_clipboard, None, CurrentTime);

    printf("\n\n=== Simple libxclip_put ===\n");
    data = "Foobarbaz";
    printf("libxclip_put with \"%s\"\n", data);
    libxclip_put(display, data, strlen(data));
    printf("> xclip -o -selection clipboard: ");
    fflush(stdout);
    system("xclip -o -selection clipboard");

    // This causes an error: Why?
    //XSync(display, False);

    //printf("\n\n=== libxclip_put with wierd data ===\n");
    //data = "\x00\x01\x02\x03\x04\x05\x06\a\b\t\n\x0a\v\f\r\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e !\"#$%&\'()*+,-./0123456789:;<=>\?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\127åäöãüαβγℕℤ😃";
    //printf("libxclip_put with \"");
    //fwrite(data, 1, 160, stdout);
    //printf("\"\n");
    //libxclip_put(display, data2, strlen(data));
    //printf("> xclip -o -selection clipboard: ");
    //fflush(stdout);
    //system("xclip -o -selection clipboard");

    //XCloseDisplay(display);

    return 0;
}
