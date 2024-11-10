#!/usr/bin/env sh

gcc -O3 -Wall -lX11 -lXmu libxclip.c test.c -o test
./test
