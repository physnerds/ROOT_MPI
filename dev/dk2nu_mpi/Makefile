CXX = `root-config --cxx`
CXXFLAGS = `root-config --cflags` -fPIC -g -DLINUX
ROOTLIBS = `root-config --glibs`
SHARED = -shared
SRCS = src/dk2nu.cc src/dkmeta.cc DKDict.cxx
HDRS = include/dkmeta.h include/dk2nu.h include/dflt.h
HDR = include
PROGRAM = libdk.so fill_dk2nu
INCLUDES = -I/nfs2/abashyal/mpich/build/include/ -I$(shell root-config --incdir)
DKINCLUDE =  -I./include
CC = mpic++
COPTS = -fPIC -DLINUX -o3 -g $(shell root-config --cflags) -m64
MPDIR = /nfs2/abashyal/mpich/build/lib
MPLIB = -lmpich -lmpi -lmpicxx -lmpl -lopa

all: libdk.so fill_dk2nu split_mpiparallelMerger

DKDict.cxx: $(HDRS) include/LinkDef.h
	@echo "Generating dictionary ..."
	@rootcint -f $@ -c -p $^

libdk.so: $(SRCS)
	@echo "Building $(PROGRAM) ..."
	@rm -f $(PROGRAM)
	@ $(CC) $(CXXFLAGS) -I$(ROOTSYS)/include -I$(HDR)  $(SHARED) -o $@ $^ $(ROOTLIBS)
	@echo "done"

%.o: %.C
	$(CC) $(COPTS) $(INCLUDES) $(DKINCLUDE) -c -o $@ $<

fill_dk2nu: % : %.o
	@ $(CC) -Wall -m64 -o $@ $< $(ROOTLIBS) -L$(MPDIR) $(MPLIB) -L/homes/abashyal/dk2nu_local -ldk

split_mpiparallelMerger: % : %.o
	@ $(CC) -Wall -m64 -o $@ $< $(ROOTLIBS) -L$(MPDIR) $(MPLIB) -L/homes/abashyal/dk2nu_local -ldk
clean:; @rm -rf core *~ DKDict* $(PROGRAM)
	@rm *.o
