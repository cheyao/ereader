To generate fonts, run:
```
clang -c rawdraw.c -o rawdraw.o
clang++ main.cpp -std=c++23 -O2 -g rawdraw.o -lm -lX11 -lGL -o render -Wno-vla-cxx-extension -I.
./render > ../src/glyphs.h
```
