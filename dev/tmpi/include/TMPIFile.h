//A TMemFile that utilizes MPI Libraries to create and Merge ROOT Files


#ifndef ROOT_TMPIFile
#define ROOT_TMPIFile

#include "TMemFile.h"
#include "TKey.h"
#include "TROOT.h"
#include "TClientInfo.h"
#include "TFileMerger.h"
#include "THashTable.h"
#include "TBits.h"
#include "/nfs2/abashyal/mpich/build/include/mpi.h"
#include <vector>
#include <memory>

class TMPIFile : public TMemFile {
public:
  int argc;char** argv;
public:
  // Int_t split;
  //Int_t np;
//now we define the constructors, destructors and other needed files here...
//constructor similar to TMemFile but some extra arguments...
//later define what to do in constructor and destructor.....
  TMPIFile(const char *name,char *buffer, Long64_t size=0,Option_t *option="",const char *ftitle="",Int_t compress=4, Int_t np=2,Int_t split=0);//at least two processors and division of subgroups
  //another constructor where it takes TMemFile pointer as an argument...
virtual ~TMPIFile();
 void R__MigrateKey(TDirectory *destination,TDirectory *source);
 void R__DeleteObject(TDirectory *dir,Bool_t withReset);

 private:
 struct ParallelFileMerger : public TObject{
 public:
   typedef std::vector<TClientInfo>ClientColl_t;
   TString fFilename;
   TBits fClientsContact;
   UInt_t fNClientsContact;
   ClientColl_t fClients;
   TTimeStamp fLastMerge;
   TFileMerger fMerger;
   ParallelFileMerger(const char *filename,Bool_t writeCache=kFALSE);
   virtual ~ParallelFileMerger();
   ULong_t Hash() const;
   const char *GetName()const;
   Bool_t InitialMerge(TFile *input);
   Bool_t Merge();
   Bool_t NeedFinalMerge();
   TClientInfo tcl;
 }; 

  MPI_Comm SplitMPIComm(MPI_Comm source,int comm_no);

 ClassDef(TMPIFile,0)

};
#endif
