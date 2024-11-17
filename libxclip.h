//    libxclip -- If xclip / xsel was a C library
//    Copyright (C) 2024  Emma Bastås
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



#ifndef LIBXCLIP_H_
#define LIBXCLIP_H_
#include <unistd.h>
#include <X11/Xlib.h>
typedef struct PutOptions PutOptions;
struct GetOptions {
    Atom selection;
    Atom target;
    int timeout;  // in milliseconds
};
int libxclip_put(Display *display, char *data, size_t len, PutOptions *options);
int libxclip_targets(Display *display,
                     Atom **targets_ret,
                     unsigned long *nitems_ret,
                     struct GetOptions *options);
int libxclip_get(Display *display,
                 char **data_ret,
                 size_t *size_ret,
                 struct GetOptions *options);
#endif  // LIBXCLIP_H_
