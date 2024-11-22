# `libxclip`

If `xclip` / `xsel` was a C library.

## Motivation

You're writing some X11 application in C/C++ and want "CTRL-C, CTRL-V functionality". It should be pretty simple, right? I imagine it should go something like this:

```
*user presses CTRL-C*
myawesomeprogram: "Hey XServer, can you put this <data> on the clipboard?"
XServer:          "Totally, thnx!"

*time passes*

*user presses CTRL-V*
firefox.          "Hey XServer, can you let me see what's on the clipboard?"
XServer:          "Of course, here's some <data>."
```

WRONG! It is much more complicated than that, I'm sorry </3. This library tries to hide that complexity behind a single `libxclip_put` function, that's all this does.

## Usage

**Put something on the clipboard**

```C
#include <X11/Xlib.h>
#indluce "libxclip.h"

void main() {
    display *display = XOpenDisplay(NULL);
    char *data = "Hello, World!";
    libxclip_put(display, data, len(data), NULL);
    // if the use does ctrl-p in some other application it will paste "Hello, World!"
}
```

This even works *after* the C program terminates, because (now comes some technical X11 language) a child process remains and responds to `SelectionRequest`'s until it loses ownership of the selection.

The functions signature is as follows:
```C
int libxclip_put(Display *display, char *data, size_t len, libxclip_putopts *options);
```

where

- `display` The connection to the XServer.
- `data` Points to the data that you want to "put on the clipboard".
- `len` The size of `data` in number of bytes.
- `libxclip_putopts` Can be used in future versions to pass options to `libxclip_put`. Right now it does nothing and you can just pass in `NULL`.

You as the caller is responsible for freeing `data` when you no longer need it. `libxclip_put` copies `data` to memory it owns (on modern Linux: does a copy-on-write of `data`. [See this post](https://stackoverflow.com/questions/27161412/how-does-copy-on-write-work-in-fork)) and so you need not worry about freeing `data` before `libxclip_put` is done with it.

Similarly `XClose(display)` won't cause problems.

**Retrieve something from the clipboard**

```C
#include <X11/Xlib.h>
#include <stdio.h>
#indluce "libxclip.h"

void main() {
    display *display = XOpenDisplay(NULL);
    char *data;
    size_t size;
    libxclip_get(display, &data, &size, NULL);
    printf("Got \"");
    fwrite(data, 1, size, stdout);
    printf("\" from the clipboard");
}
```

The functions signature is as follows:
```C
int libxclip_get(Display *display, char **data_ret, size_t *size_ret, struct libxclip_getopts *options);
```

where

- `display` The connection to the XServer.
- `data_ret` A pointer to the clipboard contents will be written to this argument.
- `size_ret` The size of the buffer pointed to by `data_ret` will be written to this argument.
- `libxclip_getops` Can be used to pass options to `libxclip_get`. See bellow for information. `NULL` in interpreted as the default options.

`libxclip_getopts` has the following field
- `Atom selection` The selection you want to retrieve. Defaults to `None` which is interpreted as the clipboard selection (`XInternAtom(display, "CLIPBOARD", False);`).
- `Atom target` The target format you want to retrieve the contents in. Defaults to `None` which is interpreted as `XInernAtom(display, "UTF8_STRING", False);`.
- `int timeout` After `timeout` amount of milliseconds has elapsed `libxclip_get` will return with `-1`. To avoid indefinite blocking if the selection owner is ill-behaved. Defaults to `-1` which means no timeout.

You can initialize a `struct libxclip_getopts` to these values with `libxclip_getopts_initialize(struct libxclip_getopts *options)`.

You as the caller is responsible for freeing `data_ret` when you no longer need it.

### return value

`0` If the call was a success, and `-1` otherwise, for instance if there was no selection owner or you supplied some invalid options.

**Listing the available targets**

In X11 it's possible for a user to copy many different types of data, for instance you can copy text but you can also copy an image. Your program may want to behave differently depending on what type of contents is on the clipboard, and for that you can request the available "targets" with `libxclip_targets` which has the following signature

`int libxclip_targets(Display *display, Atom **targets_ret, unsigned long *nitems_ret, struct libxclip_getopts *options);`

This is similar to `libxclip_get` but you instead get back a list of `Atom`s which tell you the format of the data. So you could call `libxclip_targets` and then see what atoms are returned to determine your programs behaviour. For instance, if one of the targets is the same as `XInternAtom(display, "image/png", False);` then you might assume that the user copied an image and not text.

## Installing

Right now there is no packaging for any linux distro (maybe you can help me with that?), but this utility is very small. I suggest you do the following

1) Copy `libxclip.c` and `libxclip.h` into you project.
2) Add `#include "libxclip.h"` wherever you use it.
3) Make sure you have required dependencies installed (`libX11`)
4) Whatever command you use to compile you project, add `libclip.c` as an input file, and add a `-lX11` flag.

For instance, I'm compiling this repository's test-suite with

```sh
gcc -Og -Wall -Wno-unused-result -lX11 libxclip.c test.c -o test
```

These "installation" instruction are not very clear, I'm sorry.. Just ask me if you'd like help.

## Goals and non-goals

The main goal is to provide a nice-to-use utility function so that the author of an X11 application can focus on other things than implementing an X11 protocol.

Of course this utility should also be bug-free and introduce no vulnerabilities.

Full control over what's happening, optimal performance and support for niche use-cases is *not* a goal. If you want any of these it might be best to just use X11 functions directly, optionally looking at the source-code of this repository for guidance.

That said, if you find something to be missing I'd actually be happy if you opened an issue for it :-)

## Alternatives

As far as I'm aware there are two classes of alternatives:
- Use some UI framework like [Qt](https://doc.qt.io/) which provides similar utility functions for you.
- Use `system()` to call a CLI-tool like [xclip](https://github.com/astrand/xclip) or [xsel](https://github.com/kfish/xsel) that can do this for you.

## Contributing

Yes please :-) If you want to implement some feature and so on, please don't hesitate to reach out to me.

## License

The source code is based off of [xclip](https://github.com/astrand/xclip) which is licensed under GNU GPLv2 or later. The code in this repository is (re)licensed under GNU GPLv3 or later.

## TODO

- [ ] Better "installation" instructions
- [ ] Package for common Linux distros
- [ ] Implement a `libxclip_get`?
- [ ] Proper documentation (like a man-page?)
- [X] Do the licensing properly (add LICENSE file and license comments on top of files)
