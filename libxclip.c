#include "libxclip.h"

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdio_ext.h> // for __fpurge
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xmu/Atoms.h> // TODO remove this an use XInternAtom instead

// #define DEBUG

// pid of the child process, of 0 if we're still in the parent process
pid_t pid = 0;

enum TransferState {
    STATE_NONE = 0,
    STATE_INCR = 1,
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
    Atom property;
    size_t bytes_transfered;
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
    new_transfer->property = 0;
    new_transfer->bytes_transfered = 0;
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

void xclipboard_respond(XEvent request, Atom property, Atom selection, Atom target) {
    XEvent response;

    //  Perhaps FIXME: According to ICCCM section 2.5, we should
    //  confirm that XChangeProperty succeeded without any Alloc
    //  errors before replying with SelectionNotify. However, doing
    //  so would require an error handler which modifies a global
    //  variable, plus doing XSync after each XChangeProperty.

    if (request.type == SelectionRequest) {
        response.xselection.property  = property;
        response.xselection.type      = SelectionNotify;
        response.xselection.display   = request.xselectionrequest.display;
        response.xselection.requestor = request.xselectionrequest.requestor;
        response.xselection.selection = selection;
        response.xselection.target    = target;
        response.xselection.time      = request.xselectionrequest.time;

        XSendEvent(request.xselectionrequest.display,
                   request.xselectionrequest.requestor,
                   True,
                   0,
                   &response);
    }
    else if (request.type == PropertyNotify) {
        response.xselection.property  = property;
        response.xselection.type      = SelectionNotify;
        response.xselection.display   = request.xproperty.display;
        response.xselection.requestor = request.xproperty.window;
        response.xselection.selection = selection;
        response.xselection.target    = target;
        response.xselection.time      = request.xproperty.time;

        XSendEvent(request.xproperty.display,
                   request.xproperty.window,
                   True,
                   0,
                   &response);
    }
    else {
        assert(False);
    }

    XFlush(request.xselectionrequest.display);
    // TODO what errors can this generate?
}

pid_t libxclip_put(Display *display, char *data, size_t len) {

    // The first thing we do, in an attempt to avoid race conditions,
    // missed events, and so on, is to create the child process and then have the
    // parent process freeze until the child process has performed all it's setup

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

    // We'll use these pipes for the child process to thell the parent that it can
    // resume.
    int pipefd[2];
    int ret = pipe(pipefd);
    if (ret == -1) {
        assert(False);
    }

    pid = fork();
    if (pid != 0) {
        #ifdef DEBUG
        printf("Waiting for child process to setup before returning to called\n");
        #endif

        char buf;
        read(pipefd[0], &buf, 1);

        #ifdef DEBUG
        printf("Child process is done with setup\n");
        #endif

        close(pipefd[0]);
        close(pipefd[1]);

        return pid;
    }

    // Now that we're in the child process we re-open the connection to the display
    // I'm not sure how this all works, if this is the correct thing to do. All I
    // know is if I don't have this I run into problems and StackOverflow comments
    // suggest that you "need one XOpenDisplay per thread", and that almost what
    // we're doing here
    Display *parent_display = display;
    display = XOpenDisplay(XDisplayString(parent_display));

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

    // when fork() creates the child process it copies the stack and the heap
    // from the parent process, including the stdout buffer. This means that
    // the (copied) stdout buffer is flushed when the child process terminates,
    // in some cases resulting in mysterious "double printing". So the child
    // process starts by clearing the outout buffer to avoid this.
    __fpurge(stdout);

    // Move into root, so that we don't cause any problems in case the directory
    // we're currently in needs to be unmounted
    int sucess = chdir("/");
    if (sucess == -1) {
        #ifdef DEBUG
        printf("Failed to move child process into root directory!?\n");
        #endif
    }

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

    // The head of the linked list that keeps track of all ongoing INCR transfers.
    struct transfer *transfers = NULL;

    // Intern some atoms
    const Atom A_CLIPBOARD = XInternAtom(display, "CLIPBOARD", False);
    const Atom A_TARGETS = XInternAtom(display, "TARGETS", False);
    const Atom A_UTF8_STRING = XInternAtom(display, "UTF8_STRING", False);
    const Atom A_INCR = XInternAtom(display, "INCR", False);

    // Now we're ready for the parent process to return to the caller
    // TODO: We can probably let the parent resume earlier than this, but let's
    // stay safe for now
    XSync(display, False);
    write(pipefd[1], "1", 1); // Notify parent
    close(pipefd[0]);
    close(pipefd[1]);

    XEvent event;
    while (True) {
        XNextEvent(display, &event);
        #ifdef DEBUG
        printf("Got an event\n");
        #endif

        // FIXME: ICCCM 2.2: check evt.time and refuse requests from
        // outside the period of time we have owned the selection.

        // We have lost ownership of the selection (for instance the user did a
        // CTRL-C in some other application).
        // There is nothing more for us to do, except complete any ongoing transfers
        if (event.type == SelectionClear) {
            #ifdef DEBUG
            printf("Got a SelectionClear\n");
            #endif

            _Exit(3);

            // TODO handle remaining transfers
        }

        int target = 0; // Initialized only to avoid -Wmaybe-uninitialized
        #ifdef DEBUG
        char *target_name = "";
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
            && target == A_TARGETS) {
            #ifdef DEBUG
            printf("Got a selection request with target = TARGETS\n");
            #endif

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
                A_TARGETS,
                A_UTF8_STRING,
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
            xclipboard_respond(event,
                               event.xselectionrequest.property,
                               A_CLIPBOARD,
                               XInternAtom(display, "TARGETS", False));

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

            XChangeProperty(display,
                            event.xselectionrequest.requestor,
                            event.xselectionrequest.property,
                            XInternAtom(display, "UTF8_STRING", False),
                            8,
                            PropModeReplace,
                            (unsigned char *) data,
                            (int) len);
            // TODO: XChangeProperty() can generate BadAlloc, BadAtom, BadMatch, BadValue, and BadWindow errors.

            xclipboard_respond(event,
                               event.xselectionrequest.property,
                               A_CLIPBOARD,
                               XInternAtom(display, "UTF8_STRING", False));

            continue;
        }

        // The requestor asked us the send the contents of the selection as a UTF8
        // string, and we have to send it in multiple chunks.
        if (event.type == SelectionRequest
            && target == XInternAtom(display, "UTF8_STRING", False)
            && len > chunk_size) {
            #ifdef DEBUG
            printf("Got a selection request with target = %s but we can't send the response in one chunk\n", target_name);
            #endif

            // FIXME: instead of sending zero items we should send an integer
            //        representing the lower bound on the number of bytes to send
            //        ICCCM 2.7.2 INCR Properties.
            XChangeProperty(display,
                            event.xselectionrequest.requestor,
                            event.xselectionrequest.property,
                            A_INCR,
                            32,
                            PropModeReplace,
                            0,
                            0);

            // With the INCR mechanism, we need to know
            // when the requestor window changes (deletes)
            // its properties.
            XSelectInput(display, event.xselectionrequest.requestor, PropertyChangeMask);

            xclipboard_respond(event,
                               event.xselectionrequest.property,
                               A_CLIPBOARD,
                               XInternAtom(display, "UTF8_STRING", False));

            // We register that we have a new transfer in progress
            struct transfer *t = get_transfer(&transfers, event.xselectionrequest.requestor);

            // Is there an ongoing transfer already?
            if (t->state != STATE_NONE) {
                // TODO: handle somehow
                // assert(False); TODO: add me back
            }

            t->state = STATE_INCR;
            t->property = event.xselectionrequest.property; // ??

            continue;
        }

        // It _may_ be the case that some requestor is asking us to send another
        // chunk
        if (event.type == PropertyNotify
            && event.xproperty.state == PropertyDelete) {
            #ifdef DEBUG
            printf("Got a PropertyNotify and it's state field is PropertyDelete\n");
            #endif

            struct transfer *t = get_transfer(&transfers, event.xproperty.window);
            if (t->state == STATE_NONE) {
                #ifdef DEBUG
                printf("PropertyNotify is not concearning an ongoing transfer of ours, not interested.\n");
                #endif
                delete_transfer(&transfers, t);
                continue;
            }

            // TODO: handle
            if (t->state != STATE_INCR) {
                assert(False);
            }

            // This should never happen
            if (len < t->bytes_transfered) {
                assert(False);
            }

            size_t left_to_transfer = len - t->bytes_transfered;
            size_t this_chunk_size = chunk_size;
            unsigned char *this_data = (unsigned char*) data + t->bytes_transfered;

            if (left_to_transfer == 0) {
                this_chunk_size = 0;
                this_data = 0;
            } else if (left_to_transfer < chunk_size) {
                this_chunk_size = left_to_transfer;
            }

            XChangeProperty(display,
                            event.xproperty.window,
                            t->property,
                            XInternAtom(display, "UTF8_STRING", False),
                            8,
                            PropModeReplace,
                            this_data,
                            (int) this_chunk_size);

            t->bytes_transfered = t->bytes_transfered + this_chunk_size;

            xclipboard_respond(event,
                               t->property,
                               A_CLIPBOARD,
                               XInternAtom(display, "UTF8_STRING", False));

            XSync(display, False);

            continue;
        }

        // The target is not something that we support
        if (event.type == SelectionRequest
            && event.xselectionrequest.target != XInternAtom(display, "TARGETS", False)
            && event.xselectionrequest.target != XA_UTF8_STRING(display)) {
            #ifdef DEBUG
            printf("Got a selection request with target = %s. We do not support this target\n", target_name);
            #endif

            xclipboard_respond(event,
                               None,
                               A_CLIPBOARD,
                               event.xselection.target);

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
