#!/bin/bash
#COBALT -n
#COBALT -A AtlasADSP
#COBALT -t 40
#COBALT -q default

source /home/abashyal/abashyal/root_install/bin/thisroot.sh
export LD_LIBRARY_PATH=/home/abashyal/abashyal/root_mpi/v1.6/dev/tmpi/stable/lib:$LD_LIBRARY_PATH
SECONDS=0
aprun -cc depth -d 1 -j 1 /home/abashyal/abashyal/root_mpi/v1.6/dev/tmpi/stable/bin/test4 
elapsedseconds=$(( SECONDS ))
echo "$elapsedseconds"
wait
