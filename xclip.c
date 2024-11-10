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

enum RequestorContext {
    CTX_NONE         = 0,
    CTX_SENT_REQUEST = 1,
    CTX_TRANSFERING  = 2,
    CTX_BAD_TARGET   = 3,
};

// This data structure represents some other program that is in the process
// of talking to us, maybe it requested the selection, and maybe we have to
// send it in chunks to them for instance.
typedef struct requestor
{
    // Every requestor has a display and a window associated with them,
    // this is what we'll use to associate specific events to a requestor
    Display *display; // TODO: display is probably superflous
    Window window;

    // This represents what our
    enum RequestorContext context;

    // unsigned long sel_pos;?
    // int finished;?

    // In the case that the selection contents are too large to send in one
    // go we need to send them in chuks, whose size is determined by this
    // field
    long chunk_size;

    // We let all of our requestors form a linked list
    struct requestor *next;
} REQUESTOR;

static struct requestor *first_requestor = NULL;

struct requestor *get_requestor(Display *display, Window window) {
    struct requestor *current = first_requestor;

    while (current != NULL) {
        if (current->display == display && current->window == window) {
            #ifdef DEBUG
            printf("get_requestor: found an existing requestor with display: %p and window: %ld\n", display, window);
            #endif

            return current;
        }
        current = current->next;
    }

    #ifdef DEBUG
    printf("get_requestor: Regestering a new requestor with dsplay: %p and window: %ld\n", display, window);
    #endif

    // We have met a new requestor, and so we create an entry for them!
    struct requestor *new_requestor = malloc(sizeof(struct requestor));
    new_requestor->display = display;
    new_requestor->window = window;
    new_requestor->context = CTX_NONE;
    new_requestor->next = first_requestor;
    first_requestor = new_requestor;

    return first_requestor;
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

            int requestor_window = event.xselectionrequest.requestor;
            // this registers the requestor if it's not already registered
            get_requestor(display, requestor_window);
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
                            requestor_window,
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

            int requestor_window = event.xselectionrequest.requestor;
            // this registers the requestor if it's not already registered
            get_requestor(display, requestor_window);
            /* FIXME: ICCCM 2.2: check evt.time and refuse requests from
             * outside the period of time we have owned the selection. */

            XChangeProperty(display,
                            requestor_window,
                            event.xselectionrequest.property,
                            XInternAtom(display, "UTF8_STRING", False),
                            8,
                            PropModeReplace,
                            (unsigned char *) data,
                            (int) len);
            // TODO: XChangeProperty() can generate BadAlloc, BadAtom, BadMatch, BadValue, and BadWindow errors.

            xclipboard_respond(event, event.xselectionrequest.property);

            // TODO: remove the requestor from the list of active requestors

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
