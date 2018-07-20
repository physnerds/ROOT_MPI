#!/bin/bash
#COBALT -n
#COBALT -t 30
#COBALT -A AtlasADSP
#COBALT -q debug-flat-quad

source /home/abashyal/abashyal/root_install/bin/thisroot.sh
export LD_LIBRARY_PATH=/home/abashyal/abashyal/root_mpi/v1.5/dev/tmpi/stable/lib:$LD_LIBRARY_PATH
SECONDS=0
aprun  -cc depth -d 1 -j 1 /home/abashyal/abashyal/root_mpi/v1.5/dev/tmpi/stable/bin/test
elapsedseconds=$(( SECONDS ))
echo "ElapsedTime= $elapsedseconds"

