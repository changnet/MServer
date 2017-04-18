#!/bin/bash

# --enable=warning
# --enable=performance
# --enable=information
# --enable=warning,performance



cppcheck --enable=all -i../master/deps ../master
