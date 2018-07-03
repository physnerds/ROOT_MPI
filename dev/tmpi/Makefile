OBJS_LIB = $(shell ls src/*.cxx | sed '/\.cxx/.o/')
PROGS = something
INCLUDES = -I./include/ -I $(MPINCLUDES) -I$(shell root-config --incdir)
CC = $(MPIBINS)/mpic++
MPDIR = $(MPLIBS)
MPLIB = -lmpich -lmpi -lmpicxx -lmpl -lopa
ROOTLIBS = $(shell root-config --libs) -lEG


all: lib programs

lib: libTMPI.so

libTMPI.so: $(OBJS_LIB)
	if [ ! -d lib]; then mkdir -p lib;fi
	$(CC) - shared $(M32) -o lib/$@ $^

programs: $(PROGS)
	echo making $(PROGS)


$(PROGS): % : src/%.o $(OBJS_LIB) libTMPI.so
	echo obj_progrs $(OBJS_LIB)
	if [ ! -d bin ]; then mkdir -p bin; fi
	$(CC) -Wall -m64 -o bin/$@ $< $(ROOTLIBS) -L$(MPDIR) $(MPLIB) -L$(PWD)/lib/ -lTMPI


