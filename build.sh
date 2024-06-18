#!/bin/sh

mkdir -p bin

src="main.cpp"
include="-I./"
libs="-Lsteam -lsteam_api -lraylib -lm"

compiler="gcc"
exe="./bin/main"

bear -- g++ -o $exe $src $include $libs

if [ "$1" = "run" ]; then
    export LD_LIBRARY_PATH=./steam:$LD_LIBRARY_PATH
    ./$exe
fi
