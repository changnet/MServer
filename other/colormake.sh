#!/bin/bash

# Colored Make 2016.1.21
# Copyright (c) 2014 Renato Silva
# Licensed under BSD

# https://github.com/renatosilva/colormake

# Enable 256 colors for MinTTY in MSYS2
if [[ "$MSYSCON" = mintty* && "$TERM" = *256color* ]]; then
    red="\e[38;05;9m"
    green="\e[38;05;76m"
    blue="\e[38;05;74m"
    cyan="\e[0;36m"
    purple="\e[38;05;165m"
    yellow="\e[0;33m"
    gray="\e[38;05;244m"
else
    red="\e[1;31m"
    green="\e[1;32m"
    blue="\e[1;34m"
    cyan="\e[1;36m"
    purple="\e[1;35m"
    yellow="\e[1;33m"
    gray="\e[1;30m"
fi
normal="\e[0m"

# Errors, warnings, notes and compiler recipes
error="s/(^error|^.*[^a-z]error:)/$(printf $red)\\1$(printf $normal)/i"
warning="s/(^warning|^.*[^a-z]warning:)/$(printf $yellow)\\1$(printf $normal)/i"
make="s/^make(\[[0-9]+\])?:/$(printf $blue)make\\1:$(printf $normal)/"
compiler_recipe="s/^((gcc|g\+\+|clang)(.exe)? .*)/$(printf $gray)\\1$(printf $normal)/"

if [[ $(uname -or) != 1.*Msys ]]; then
    command "${COLORMAKE_COMMAND:-make}" "$@" 2> >(sed -ru -e "$warning" -e "$error" -e "$make" -e "$compiler_recipe") \
                                               > >(sed -ru -e "$warning" -e "$error" -e "$make" -e "$compiler_recipe")
else
    # MinGW MSYS does not support process substitution
    command "${COLORMAKE_COMMAND:-make}" "$@" 2>&1 | sed -ru -e "$warning" -e "$error" -e "$make" -e "$compiler_recipe"
fi
