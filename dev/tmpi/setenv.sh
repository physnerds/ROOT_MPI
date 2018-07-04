MPIINCLUDES=/nfs2/abashyal/mpich/build/include/
MPLIBS=/nfs2/abashyal/mpich/build/lib
CURDIR=$PWD


export MPINCLUDES
#export MPIBINS
export MPLIBS
export CURDIR
export LD_LIBRARY_PATH=$CURDIR/lib:$LD_LIBRARY_PATH
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
