OBJS_LIB = $(shell ls src/*.cxx | sed 's/\.cxx/.o/')
PROGS = $(shell ls src/*.C | sed 's/\.C//' | sed 's/src\///')
CC = mpic++
MPDIR = $(MPLIBS)
MPINCLUDEPATH = /homes/abashyal/mpich/build/include
INCLUDES = -I./include -I$(MPINCLUDESPATH) -I$(shell root-config --incdir)
MPLIB = -lmpich -lmpi -lmpicxx -lmpl -lopa
ROOTLIBS = $(shell root-config --libs) -lEG
COPTS = -fPIC -DLINUX -O0 -g $(shell root-config --cflags) -m64
INCLUDE = $(shell ls include/TMPIFile.h)
INCLUDE += $(shell ls include/TClientInfo.h)
MPINCLUDES = $(shell ls $(MPINCLUDEPATH)/*.h)
all: lib programs

lib: libTMPI.so

libTMPI.so: MPIDict.o $(OBJS_LIB)
	if [ ! -d lib ]; then mkdir -p lib; fi

	$(CC) -shared -m64 -o lib/$@ $^

programs: $(PROGS)
	echo making $(PROGS)


$(PROGS): % : src/%.o MPIDict.o  $(OBJS_LIB) libTMPI.so
	echo obj_progs $(OBJS_LIB)
	if [ ! -d bin ]; then mkdir -p bin; fi
	$(CC) -Wall -m64 -o bin/$@ $< $(ROOTLIBS) -L$(MPDIR) $(MPLIB) -L$(CURDIR)/lib -lTMPI

%.o: %.cxx
	$(CC) $(COPTS) $(INCLUDES) -c -o $@ $<

%.o: %.C
	$(CC) $(COPTS) $(INCLUDES) -c -o $@ $<

MPIDict.cxx: $(INCLUDE)  include/Linkdef.h 
	@echo "Generating MPI Dictionary..."
	@rootcint -f $@ -c -p $^

clean: 
	-rm src/*.o;
	rm -rf lib;
	rm -rf bin;
	-rm *.cxx;
	-rm *.pcm;
	-rm *.o
