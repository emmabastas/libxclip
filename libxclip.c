#include "libxclip.h"

#include <stdlib.h>
#include <assert.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xmu/Atoms.h> // TODO remove this an use XInternAtom instead

#define DEBUG

#ifdef DEBUG
#include <stdio.h>
#endif

enum TransferState {
    STATE_NONE         = 0,
    STATE_SENT_REQUEST = 1,
    STATE_TRANSFERING  = 2,
    STATE_BAD_TARGET   = 3,
};

// The selection we hold may be so large we have to transfer it in chunks, in which
// case we have to keep track of our ongoing transfers. We do this with this struct,
// which forms a linked list
struct transfer
{
    // The window associated with the requestor, this should be all that's needed
    // to uniquely identify a requestor
    Window requestor_window;
    enum TransferState state;
    struct transfer *next;
};

struct transfer *get_transfer(struct transfer **first_transfer, Window requestor_window) {
    struct transfer *current = *first_transfer;

    while (current != NULL) {
        if (current->requestor_window == requestor_window) {
            #ifdef DEBUG
            printf("get_transfer: found an existing requestor with window: %ld\n", requestor_window);
            #endif

            return current;
        }
        current = current->next;
    }

    #ifdef DEBUG
    printf("get_transfer_state: Regestering a new requestor with window: %ld\n", requestor_window);
    #endif

    struct transfer *new_transfer = malloc(sizeof(struct transfer));
    new_transfer->requestor_window = requestor_window;
    new_transfer->state = STATE_NONE;
    new_transfer->next = *first_transfer;
    *first_transfer = new_transfer;

    return *first_transfer;
}

void delete_transfer(struct transfer **first_transfer, struct transfer *transfer) {
    if ((*first_transfer)->requestor_window == transfer->requestor_window) {
        free(*first_transfer);
        *first_transfer = NULL;
    }

    struct transfer *past = NULL;
    struct transfer *current = *first_transfer;
    while (current != NULL) {
        if (current->requestor_window == transfer->requestor_window) {
            past->next = current->next;
            free(current);
            return;
        }
        past = current;
        current = current->next;
    }

    assert(False);
}

void xclipboard_respond(XEvent request, Atom property) {
    XEvent response;

    //  Perhaps FIXME: According to ICCCM section 2.5, we should
    //  confirm that XChangeProperty succeeded without any Alloc
    //  errors before replying with SelectionNotify. However, doing
    //  so would require an error handler which modifies a global
    //  variable, plus doing XSync after each XChangeProperty.

    // Set values for the response
    response.xselection.property  = property;
    response.xselection.type      = SelectionNotify;
    response.xselection.display   = request.xselectionrequest.display;
    response.xselection.requestor = request.xselectionrequest.requestor;
    response.xselection.selection = request.xselectionrequest.selection;
    response.xselection.target    = request.xselectionrequest.target;
    response.xselection.time      = request.xselectionrequest.time;

    // send the response event
    XSendEvent(request.xselectionrequest.display,
               request.xselectionrequest.requestor,
               True,
               0,
               &response);
    // TODO what errors can this generate?

    XFlush(request.xselectionrequest.display);
    // TODO what errors can this generate?
}

pid_t libxclip_put(Display *display, char *data, size_t len) {
    // Determine chunk_size
    // In the case that the selections contents is very large we may
    // have to send the clipboard selection in multiple chunks,
    // and the maximum chink size is defined by X11
    //
    // We consider selections larger than a quarter of the maximum
    // request size to be "large". See ICCCM section 2.5
    // FIXME: I think the chunk_size should be ~16x the size of what we currently do
    //
    // First see if X supports extended-length encoding, it returns 0 if not
    size_t chunk_size = XExtendedMaxRequestSize(display) / 4;
    // Otherwise, try the normal encoding
    if (!chunk_size) { chunk_size = XMaxRequestSize(display) / 4; }
    // If this fails for some reason, we fallback to this
    if (!chunk_size) { chunk_size = 4096; }

    // A dummy window that exists only for us to intercept `SelectionRequest` events
    Window window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 1, 1, 0, 0, 0);
    // TODO: XCreateSimpleWindow can generate BadAlloc, BadMatch, BadValue, and BadWindow errors.
    // https://tronche.com/gui/x/xlib/window/XCreateWindow.html

    // take control of the selection so that we receive
    // `SelectionRequest` events from other windows
    // FIXME: Should not use CurrentTime, according to ICCCM section 2.1
    XSetSelectionOwner(display, XA_CLIPBOARD(display), window, CurrentTime);
    // TODO: What errorrs can this generate?

    // Double-check SetSelectionOwner did not "merely appear to succeed"
    if (XGetSelectionOwner(display, XA_CLIPBOARD(display)) != window) {
        assert(False);
        // TODO handle error
    }
    // TODO: Can XGetSelectionOwner generate an error.

    XSelectInput(display, window, PropertyChangeMask);
    // TODO: XSelectInput() can generate a BadWindow error.
    // https://tronche.com/gui/x/xlib/event-handling/XSelectInput.html

    // Now it's time to create a separate process that hangs around waiting for other
    // programs to make selection requests.
    //
    // NB. The selections contents are stored in `data` and the child process will of
    // course read from this. What happens though in the case that the parent process
    // exits before the child process is finished? One might think that all data all-
    // ocated by the parent process is freed, including what `data` points to. This
    // is true in some sense, but `fork` performs a copy-on-write duplication on all
    // of the heap contents from the parent process to the child process. This means:
    // 1) If the parent process never writes to `data` after having called
    //    `xlipboard_persit` then no copies are made of that data, yet the child
    //    process still has acess to it after the parent process exited. AWESOME!
    // 2) If the parent process does write then the `data` is copied, and the child
    //    process will continue to acess the original contents.
    // See: https://unix.stackexchange.com/questions/155017/does-fork-immediately-copy-the-entire-process-heap-in-linux
    // THAT'S SO COOL

    pid_t pid = fork();
    if (pid != 0) {
        // I don't understand how this works; In my mind, we make this function call
        // we tell the X11 server that no one owns the clipboard any more, but this
        // line is included in `xclip` when they fork, and it works.. and it works
        // here to, but why??
        XSetSelectionOwner(display, XA_CLIPBOARD(display), None, CurrentTime);
        // TODO: What errors can this generate?
        return pid;
    }

    // Move into root, so that we don't cause any problems in case the directory
    // we're currently in needs to be unmounted
    int sucess = chdir("/");
    if (sucess == -1) {
        #ifdef DEBUG
        printf("Failed to move child process into root directory!?\n");
        #endif
    }

    XEvent event;
    while (True) {
        XNextEvent(display, &event);

        // We have lost ownership of the selection (for instance the user did a
        // CTRL-C in some other application).
        // There is nothing more for us to do, except complete any ongoing transfers
        if (event.type == SelectionClear) {
            #ifdef DEBUG
            printf("Got a SelectionClear\n");
            #endif
            // TODO
            return 0;
        }

        int target;
        #ifdef DEBUG
        char *target_name;
        #endif
        if (event.type == SelectionRequest) {
            target = event.xselectionrequest.target;

            #ifdef DEBUG
            target_name = XGetAtomName(display, event.xselectionrequest.target);
            // this is a memory leak but whatever we're in debug
            #endif
        }

        // Some program asked us what kinds of formats (i.e. targets) we can send
        // the selection contents in (like utf8, html, png, etc.).
        // This can happen for instance when a user does CTRL-V in an application,
        // usually the application wants to know what format the content is in, for
        // instance if we support a png target maybe the application would like to
        // insert an image instead of text for the user.
        if (event.type == SelectionRequest
            && target == XInternAtom(display, "TARGETS", False)) {
            #ifdef DEBUG
            printf("Got a selection request with target = TARGETS\n");
            #endif

            /* FIXME: ICCCM 2.2: check evt.time and refuse requests from
             * outside the period of time we have owned the selection. */

            // This is the contents of our resonse.
            // We supports two targets
            // 1) The TARGETS target (duh)
            // 2) UTF8_STRING
            // TODO: Should we support more targets by default?
            // Some reasonable targets could be:
            // - STRING
            // - TEXT
            // - text/plain
            // - text/plain;charset=utf-8
            Atom types[2] = {
                XInternAtom(display, "TARGETS", False),
                XInternAtom(display, "UTF8_STRING", False)
            };

            // put the response contents into the request's property
            XChangeProperty(display,
                            event.xselectionrequest.requestor,
                            event.xselectionrequest.property,
                            XInternAtom(display, "ATOM", False),
                            32,
                            PropModeReplace,
                            (unsigned char *) types,
                            (int) (sizeof(types) / sizeof(Atom)));
            // TODO: XChangeProperty() can generate BadAlloc, BadAtom, BadMatch, BadValue, and BadWindow errors.

            // Now we send the response
            xclipboard_respond(event, event.xselectionrequest.property);

            continue;
        }

        // The requestor asked us the send the contents of the selection as a UTF8
        // string, and we can send the contents in one chunk
        if (event.type == SelectionRequest
            && target == XInternAtom(display, "UTF8_STRING", False)
            && len <= chunk_size) {
            #ifdef DEBUG
            printf("Got a selection request with target = %s and we can send the response in one chunk\n", target_name);
            #endif

            /* FIXME: ICCCM 2.2: check evt.time and refuse requests from
             * outside the period of time we have owned the selection. */

            XChangeProperty(display,
                            event.xselectionrequest.requestor,
                            event.xselectionrequest.property,
                            XInternAtom(display, "UTF8_STRING", False),
                            8,
                            PropModeReplace,
                            (unsigned char *) data,
                            (int) len);
            // TODO: XChangeProperty() can generate BadAlloc, BadAtom, BadMatch, BadValue, and BadWindow errors.

            xclipboard_respond(event, event.xselectionrequest.property);

            continue;
        }

        if (event.type == SelectionRequest
            && target == XInternAtom(display, "UTF8_STRING", False)
            && len > chunk_size) {
            #ifdef DEBUG
            printf("Got a selection request with target = %s but we can't send the response in one chunk\n", target_name);
            #endif

            // TODO: implement
            xclipboard_respond(event, None);

            continue;
        }

        // The target is not something that we support
        if (event.type == SelectionRequest
            && event.xselectionrequest.target != XInternAtom(display, "TARGETS", False)
            && event.xselectionrequest.target != XA_UTF8_STRING(display)) {
            #ifdef DEBUG
            printf("Got a selection request with target = %s. We do not support this target\n", target_name);
            #endif

            xclipboard_respond(event, None);

            continue;
        }

        if (event.type == PropertyNotify) {
            #ifdef DEBUG
            printf("Got a PropertyNotify\n");
            #endif

            continue;
        }

        #ifdef DEBUG
        const char *evtstr[36] = {
            "ProtocolError", "ProtocolReply", "KeyPress", "KeyRelease",
            "ButtonPress", "ButtonRelease", "MotionNotify", "EnterNotify",
            "LeaveNotify", "FocusIn", "FocusOut", "KeymapNotify", "Expose",
            "GraphicsExpose", "NoExpose", "VisibilityNotify", "CreateNotify",
            "DestroyNotify", "UnmapNotify", "MapNotify", "MapRequest",
            "ReparentNotify", "ConfigureNotify", "ConfigureRequest",
            "GravityNotify", "ResizeRequest", "CirculateNotify",
            "CirculateRequest", "PropertyNotify", "SelectionClear",
            "SelectionRequest", "SelectionNotify", "ColormapNotify",
            "ClientMessage", "MappingNotify", "GenericEvent", };
        printf("We got an unexpected %s event\n", evtstr[event.type]);
        #endif
    }

    // Let everyone know that we're no longer taking care of the selection
    XSetSelectionOwner(display, XA_CLIPBOARD(display), None, CurrentTime);

    // This return statement doesn't do anything meaningfull, because this return
    // commes from the child process. The statement is only here to have the compiler
    // not complain.
    return pid;
}
