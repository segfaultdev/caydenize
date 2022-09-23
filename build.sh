#!/usr/bin/sh

gcc $(find . -name "*.c") -Iinclude -o caydenize -O3 -lm
