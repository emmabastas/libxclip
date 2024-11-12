#include "libxclip.h"

#include <stdlib.h>
#include <sys/wait.h> // for waitpid
#include <string.h>
#include <stdio.h>
#include <stdio_ext.h> // for __fpurge
#include <X11/Xlib.h>
#include <X11/Xatom.h>

Display *display;

char *data;

Atom a_clipboard;

void _00200_test_no_double_printing() {
    printf("\n\n=== libxclip_put doesn't cause double printing\n");
    // See the comments next to __fpurge in libxclip.c for an explaination of
    // this test
    printf("libxclip_put prints: ");
    fflush(stdout);
    // "I'm in the stdout buffer" will be copied to the childs stdout buffer
    fwrite("I'm in the stdout buffer", 1, 24, stdout);
    libxclip_put(display, "", 0);
    // Remove "I'm in the stdout buffer" from this process' stdout buffer
    __fpurge(stdout);
    // XSetSelectionOwner causes the child process to exit and flush stdout.
    // We sleep so that the child process created by libxclip doesn't miss any
    // events. It shouldn't miss any events! But that not trivial and we test that
    // in the next test.
    usleep(10000);
    XSetSelectionOwner(display, a_clipboard, None, CurrentTime);
    printf("\n");
    XSync(display, False);
    usleep(10000);
}

void _00300_test_no_missed_events_in_begining() {
    printf("\n\n=== libxclip doesn't miss XEvent's in the beginning ===\n");
    printf("libxclip_put(display, \"\", 0);\n");
    pid_t pid = libxclip_put(display, "", 0);
    printf("XSetSelectionOwner(display, a_clipboard, None, CurrentTime);\n");
    XSetSelectionOwner(display, a_clipboard, None, CurrentTime);
    printf("XSync(display, False);\n");
    XSync(display, False);
    printf("Waiting for child rocess with PID %d to exit...\n", pid);
    int status;
    waitpid(pid, &status, 0); // If this never unblocks then this test failed
    printf("%d exited!\n", pid);
}

void _00400_test_simple_data() {
    printf("\n\n=== libxclip_put with some simple data ===\n");
    data = "Foobarbaz";
    printf("libxclip_put with \"%s\"\n", data);
    libxclip_put(display, data, strlen(data));
    printf("> xclip -o -selection clipboard 2>&1: ");
    fflush(stdout);
    system("xclip -o -selection clipboard 2>&1");
}

void _00500_test_wierd_data() {
    printf("\n\n=== libxclip_put with wierd data ===\n");
    data = "\x00\x01\x02\x03\x04\x05\x06\a\b\t\n\x0a\v\f\r\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e !\"#$%&\'()*+,-./0123456789:;<=>\?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\127åäöãüαβγℕℤ😃";
    printf("libxclip_put with \"");
    fwrite(data, 1, 160, stdout);
    printf("\"\n");
    libxclip_put(display, data, 160);
    printf("> xclip -o -selection clipboard 2>&1: ");
    fflush(stdout);
    system("xclip -o -selection clipboard 2>&1");
}

void _00600_multiple_puts() {
    printf("\n\n=== multiple libxclip_put in succession behaves as expected===\n");
    libxclip_put(display, "1", 1);
    libxclip_put(display, "2", 1);
    libxclip_put(display, "3", 1);
    printf("> xclip -o -selection clipboard 2>&1: ");
    fflush(stdout);
}

int main(void) {
    display = XOpenDisplay(NULL);
    a_clipboard = XInternAtom(display, "CLIPBOARD", False);

    XSetSelectionOwner(display, a_clipboard, None, CurrentTime);
    XSync(display, True);

    char buffer[100];
    memset(buffer, 0, 100);
    read(STDIN_FILENO, buffer, 100);

    if(strcmp(buffer, "00200\n") == 0) {
        _00200_test_no_double_printing();
    }

    if(strcmp(buffer, "00300\n") == 0) {
        _00300_test_no_missed_events_in_begining();
    }
    if(strcmp(buffer, "00400\n") == 0) {
        _00400_test_simple_data();
    }
    if(strcmp(buffer, "00500\n") == 0) {
        _00500_test_wierd_data();
    }
    if(strcmp(buffer, "00600\n") == 0) {
        _00600_multiple_puts();
    }

    return 0;
}
