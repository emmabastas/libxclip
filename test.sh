#!/usr/bin/env sh

gcc -O3 -Wall -Wno-unused-result -lX11 -lXmu libxclip.c test.c -o test

./test
