#!/bin/bash

echo
echo -n "Use this IP for VN: "
hostname -I | awk '{print $1}'
echo

g++ oracle-node.cpp -o out
./out oracle-node-config.txt

