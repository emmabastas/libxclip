#!/usr/bin/env sh

gcc -Og -Wall -Wno-unused-result -lX11 -lXmu libxclip.c test.c -o test

./test
