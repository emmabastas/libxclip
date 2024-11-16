//    libxclip -- If xclip / xsel was a C library
//    Copyright (C) 2024  Emma Bastås <emma.bastas@protonmail.com>
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.



#include "libxclip.h"

#include <stdlib.h>
#include <sys/wait.h> // for waitpid
#include <string.h>
#include <stdio.h>
#include <stdio_ext.h> // for __fpurge
#include <assert.h>
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
    libxclip_put(display, "", 0, NULL);
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
    pid_t pid = libxclip_put(display, "", 0, NULL);
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
    libxclip_put(display, data, strlen(data), NULL);
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
    libxclip_put(display, data, 160, NULL);
    printf("> xclip -o -selection clipboard 2>&1: ");
    fflush(stdout);
    system("xclip -o -selection clipboard 2>&1");
}

void _00600_multiple_puts() {
    printf("\n\n=== multiple libxclip_put in succession behaves as expected ===\n");
    libxclip_put(display, "1", 1, NULL);
    libxclip_put(display, "2", 1, NULL);
    libxclip_put(display, "3", 1, NULL);
    printf("> xclip -o -selection clipboard 2>&1: ");
    fflush(stdout);
    system("xclip -o -selection clipboard 2>&1");
}

void _007000_multiple_pastes() {
    printf("\n\n=== one libxclip_put can serve multiple CTRL-V's ===");
    libxclip_put(display, "hej!", 4, NULL);
    for(int i = 0; i < 3; i++) {
        printf("\n");
        printf("> xclip -o -selection clipboard 2>&1: ");
        fflush(stdout);
        system("xclip -o -selection clipboard 2>&1");
    }
}

void _008000_empty_contents() {
    printf("\n\n=== libxclip can take empy data ===\n");
    libxclip_put(display, "", 0, NULL);
    printf("> xclip -o -selection clipboard 2>&1: ");
    fflush(stdout);
    system("xclip -o -selection clipboard 2>&1");
}

void _009000_handles_TARGETS() {
    printf("\n\n=== libxclip can respond to a TARGETS request\n");
    libxclip_put(display, "", 0, NULL);
    printf("> xclip -o -selection clipboard -target TARGETS 2>&1:\n");
    system("xclip -o -selection clipboard -target TARGETS 2>&1");
}

void _010000_handles_large_data() {
    printf("\n\n=== libxclip can handle sucessively larger and larger data===\n");
    const unsigned long n = 25;
    char *buffer = malloc(1 << n);
    memset(buffer, '#', 1 << n);
    printf("buffer size (#bytes)\n");
    for(int i = 11; i <= n; i++) {
        const unsigned long len = 1 << i;
        printf("%ld: ", len);
        fflush(stdout);
        libxclip_put(display, buffer, len, NULL);
        system("xclip -o -selection clipboard | wc -c");
        printf("\n");
    }
}

void _011000_multiple_large_transfers() {
    printf("\n\n=== libxclip can handle multiple large transfers at the same time===\n");
    size_t size = 1 << 25;
    //size_t size = 32;
    char *buffer = malloc(size);
    memset(buffer, '#', size);
    libxclip_put(display, buffer, size, NULL);
    system("(xclip -o -se c | wc -c)"
           "& (xclip -o -se c | wc -c)"
           "& (xclip -o -se c | wc -c)"
           "& (xclip -o -se c | wc -c)"
           "& (xclip -o -se c | wc -c)"
           "& (xclip -o -se c | wc -c)"
           "& (xclip -o -se c | wc -c)"
           "& (xclip -o -se c | wc -c)"
           "& (xclip -o -se c | wc -c)"
           "& (xclip -o -se c | wc -c)");
    usleep(1000);  // Just to make sure the previous `system` call had time to output everyting
}

void _012000_read_and_steal() {
    printf("\n\n=== libxclip should complete ongoing transfers evevn after having lost ownership of the selection\n");

    size_t inbuffer_size = 1 << 25;
    char *inbuffer = malloc(inbuffer_size);
    memset(inbuffer, '#', inbuffer_size);

    printf("Doing libxclip_but with a buffer large enough to require INCR.\n");
    libxclip_put(display, inbuffer, inbuffer_size, NULL);

    Atom property = XInternAtom(display, "LIBXCLIP_DATA", False);

    Window window = XCreateSimpleWindow(display,
                                        DefaultRootWindow(display),
                                        0, 0, 1, 1, 0, 0, 0);

    printf("Doing XConvertSelection.\n");
    XConvertSelection(display,
                      XInternAtom(display, "CLIPBOARD", False),
                      XInternAtom(display, "UTF8_STRING", False),
                      property,
                      window,
                      CurrentTime);

    printf("Doing XNextEvent and asserting it's type is SelectionNotify.\n");
    XEvent event;
    XNextEvent(display, &event);
    assert(event.type == SelectionNotify);

    printf("Doing XGetWindowProperty and asserting the property type is INCR.\n");
    Atom property_type;
    int format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *out_buffer;

    XGetWindowProperty(display,
                       window,
                       property,
                       0,
                       4096 / 4,
                       False,
                       AnyPropertyType,
                       &property_type,
                       &format,
                       &nitems,
                       &bytes_after,
                       &out_buffer);
    assert(property_type == XInternAtom(display, "INCR", False));

    printf("Setting the selection owner to None.\n");
    XSetSelectionOwner(display, XInternAtom(display, "CLIPBOARD", False), None, CurrentTime);

    printf("Doing XDeleteProperty, signaling that we're ready for a new chunk.\n");
    XDeleteProperty(display, window, property);

    printf("Doing XNextEvent with a (2sec timeout) and asserting it's type is SelectionNotify.\n");

    for (int i = 0; i < 10; i++) {
        if(XPending(display) != 0) {
            goto gotevent;
        }
        usleep(200000);
    }
    assert(False);

    gotevent:
    XNextEvent(display, &event);
    assert(event.type == SelectionNotify);
    printf("Success!\n");
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
    if(strcmp(buffer, "00700\n") == 0) {
        _007000_multiple_pastes();
    }
    if(strcmp(buffer, "00800\n") == 0) {
        _008000_empty_contents();
    }
    if(strcmp(buffer, "00900\n") == 0) {
        _009000_handles_TARGETS();
    }
    if(strcmp(buffer, "01000\n") == 0) {
        _010000_handles_large_data();
    }
    if(strcmp(buffer, "01100\n") == 0) {
        _011000_multiple_large_transfers();
    }
    if(strcmp(buffer, "01200\n") == 0) {
        _012000_read_and_steal();
    }

    return 0;
}
