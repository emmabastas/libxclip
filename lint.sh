echo "=== Checking if 'gcc -fanalyzer -O3 -shared' has any complaints ==="
gcc -fanalyzer -O3 -lX11 -lXmu libxclip.c -shared -o /dev/null

echo "=== Checking if 'gcc -std=99' has any complaints ==="
gcc -std=c99 -O3 -lX11 -lXmu libxclip.c -shared -o /dev/null

echo "=== Checking if 'gcc -std=99 -pedantic' has any complaints ==="
gcc -std=c99 -pedantic -O3 -lX11 -lXmu libxclip.c -shared -o /dev/null
