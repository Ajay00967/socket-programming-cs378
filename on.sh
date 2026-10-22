#!/bin/bash

g++ -std=c++20 oracle-node.cpp -o oracle-node
./oracle-node oracle-node-config.txt
