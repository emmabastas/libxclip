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
int libxclip_put(Display *display, char *data, size_t len, PutOptions *options);
#endif  // LIBXCLIP_H_

