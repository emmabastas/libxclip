#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xmu/Atoms.h>

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

void xclipboard_persist(Display *display, char *data, size_t len) {
    // Determine chunk_size
    // In the case that the selections contents is very large we may
    // have to send the clipboard selection in multiple chunks,
    // and the maximum chink size is defined by X11
    //
    // We consider selections larger than a quarter of the maximum
    // request size to be "large". See ICCCM section 2.5
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
    // TODO: What error can this generate?

    // Double-check SetSelectionOwner did not "merely appear to succeed"
    if (XGetSelectionOwner(display, XA_CLIPBOARD(display)) != window) {
        assert(False);
    }
    // TODO: Can XGetSelectionOwner generate an error.

    XSelectInput(display, window, PropertyChangeMask);
    // TODO: XSelectInput() can generate a BadWindow error.
    // https://tronche.com/gui/x/xlib/event-handling/XSelectInput.html

    XStoreBuffer(display, data, len, 0);

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

        assert(False);
    }

    // Let everyone know that we're no longer taking care of the selection
    XSetSelectionOwner(display, XA_CLIPBOARD(display), None, CurrentTime);

    return;
}

int xerror_handler(Display *display, XErrorEvent *error) {
    fprintf(stderr, "ERROR!!!\n");
    return 0;
}

int xerror_handler_fatal(Display *display) {
    fprintf(stderr, "ERROR!!!\n");
    return 0;
}

int main(void) {
    XSetErrorHandler(&xerror_handler);
    XSetIOErrorHandler(&xerror_handler_fatal);
    XOpenDisplay("kajdshakjds hha");
    char *data = "Hello! :-)";
    Display *display = XOpenDisplay(NULL);
    xclipboard_persist(display, data, strlen(data));
}
