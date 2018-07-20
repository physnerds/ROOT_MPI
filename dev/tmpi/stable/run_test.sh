#!/bin/bash

SECONDS=0
mpirun -np 40 ./bin/test
elapsedseconds=$(( SECONDS ))
echo "ElapsedTime= $elapsedseconds"
