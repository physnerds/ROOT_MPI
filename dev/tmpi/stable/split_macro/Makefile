
PROGS = split_mpiparallelMerger
INCLUDES = -I /nfs2/abashyal/mpich/build/include/ -I$(shell root-config --incdir)
CC = mpic++
COPTS = -fPIC -DLINUX -o0 -g $(shell root-config --cflags) -m64
MPDIR = /nfs2/abashyal/mpich/build/lib
MPLIB = -lmpich -lmpi -lmpicxx -lmpl -lopa
ROOTLIBS = $(shell root-config --libs) -lEG

programs: $(PROGS)
	echo making $(PROGS)

$(PROGS): % : %.o
	$(CC) -Wall -m64 -o $@ $< $(ROOTLIBS) -L$(MPDIR) $(MPLIB) 

%.o: %.c
	$(CC) $(COPTS) $(INCLUDES) -c -o $@ $<

%.o: %.C
	$(CC) $(COPTS) $(INCLUDES) -c -o $@ $<
clean:
	 -rm *.o $(PROGS)
