#!/usr/bin/env sh

#    libxclip -- If xclip / xsel was a C library
#    Copyright (C) 2024  Emma Bastås <emma.bastas@protonmail.com>
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.



echo "=== Checking if 'gcc -fanalyzer -O3 -shared' has any complaints ==="
gcc -fanalyzer -O3 -lc -lX11 libxclip.c -shared -o /dev/null

echo "=== Checking if 'gcc -std=99' has any complaints ==="
gcc -std=gnu99 -O3 -lc -lX11 libxclip.c -shared -o /dev/null

echo "=== Checking if 'gcc -std=99 -pedantic' has any complaints ==="
gcc -std=gnu99 -pedantic -O3 -lc -lX11 libxclip.c -shared -o /dev/null

echo "=== Checking if cpplint has any complaits ==="
cpplint --extensions=c,h \
        --filter=-readability/todo,-readability/casting,-build/include_what_you_use,-runtime/int \
        libxclip.c libxclip.h
