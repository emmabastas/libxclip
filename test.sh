#!/usr/bin/env sh

gcc -Og -Wall -Wno-unused-result -lX11 -lXmu libxclip.c test.c -o test

echo "00200" | ./test
echo "00300" | ./test
echo "00400" | ./test
echo "00500" | ./test
echo "00600" | ./test
