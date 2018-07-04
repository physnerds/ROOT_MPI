/*
-------START OF COMMENT-----------
TMPIFile is like a TFile except it reads and writes in memory using TMPIFile and later merges them into TFile using MPI class libraries...

---------END OF COMMENT-----------
 */

#include "TMemFile.h"
#include "TError.h"
#include "TSystem.h"
#include "TROOT.h"
#include "TArrayC.h"
#include "TKey.h"
#include "TMPIFile.h"
#include "mpi.h"
/*
Will basically add all the headers from TMemFile.cxx File later...
 */
#include "mpi.h"

//Debug mode implementation later.....
//...
ClassImp(TMPIFile);
//the constructor should be similar to TMemFile...

TMPIFile::TMPIFile(const char *name, char *buffer, Long64_t size,
		   Option_t *option,const char *ftitle,Int_t compress,
		   Int_t np,Int_t split):TMemFile(name,buffer,size,ftitle,compress){
  //Initialize MPI's
  MPI_Init(&argc,&argv);
  
}

TMPIFile::~TMPIFile(){
  MPI_Finalize();
}
