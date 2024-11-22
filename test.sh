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



# TODO: Maybe we should also do a test-run with -O3 in case optimizing reveals
#       bugs to us.
gcc -Og -Wall -Wno-unused-result -lX11 libxclip.c CuTest.c test.c -o test

echo "00200" | ./test
echo "00300" | ./test
echo "00400" | ./test
echo "00500" | ./test
echo "00600" | ./test
echo "00700" | ./test
echo "00800" | ./test
echo "00900" | ./test
echo "01000" | ./test
echo "01100" | ./test
echo "01200" | ./test

echo "10000" | ./test
echo "10100" | ./test

echo "20000" | ./test
echo "20050" | ./test
echo "20060" | ./test
echo "20100" | ./test
echo "20200" | ./test
echo "20300" | ./test
echo "20400" | ./test
echo "20500" | ./test
echo "20600" | ./test

echo "30000" | ./test
