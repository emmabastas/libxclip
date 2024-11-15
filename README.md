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

```C
#include <X11/Xlib.h>
#include <stdio.h>
#indluce "libxclip.h"

void main() {
    Display *display = XOpenDispla(NULL);

    char *data = "Hello, World!";
    print("Putting some data 'in the clipboard'");
    libxclip_put(display, data, len(data), NULL);
    // If the use does CTRL-P in some other application it will paste "Hello, World!"
}
```

This even works *after* the C program terminates, because (now comes some technical X11 language) a child process remains and responds to `SelectionRequest`'s until it loses ownership of the selection.

The functions signature is as follows:
```C
int libxclip_put(Display *display, char *data, size_t len, PutOptions *options);
```

where

- `display` The connection to the XServer.
- `data` Points to the data that you want to "put on the clipboard".
- `len` The size of `data` in number of bytes.
- `PutOptions` Can be used in future versions to pass options to `libxclip_put`. Right now it does nothing and you can just pass in `NULL`.

You as the caller is responsible for freeing `data` when you no longer need it. `libxclip_put` copies `data` to memory it owns (on modern Linux: does a copy-on-write of `data`. [See this post](https://stackoverflow.com/questions/27161412/how-does-copy-on-write-work-in-fork)) and so you need not worry about freeing `data` before `libxclip_put` is done with it.

Similarly `XClose(display)` won't cause problems.

### return value

An `int` to signal if some error occurred, however, right now we don't report any errors (TODO) and so the return value is always `0` and you shouldn't pay it any mind.

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
