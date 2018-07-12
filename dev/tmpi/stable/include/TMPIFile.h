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
  MPI_Comm row_comm; //for now at least one comm to be declared..
  char fMPIFilename[1000];
public:
  TMPIFile(const char *name,char *buffer, Long64_t size=0,Option_t *option="",Int_t split = 4,const char *ftitle="",Int_t compress=4);//at least two processors and division of subgroups
  TMPIFile(const char *name, Option_t *option="",Int_t split = 4, const char *ftitle="", Int_t compress=4);
virtual ~TMPIFile();
 void R__MigrateKey(TDirectory *destination,TDirectory *source);
 void R__DeleteObject(TDirectory *dir,Bool_t withReset);
 void PurgeEveryThing();

  void SendBuffer(char *buff,int buff_size,MPI_Comm comm);
  void ReceiveAndMerge(bool cache=false,MPI_Comm=0,int rank=0,int size=0);
  void CreateBufferAndSend(TMemFile *file,bool cache=false,MPI_Comm comm=0,int sent = 0);
  void CreateBufferAndSend(bool cache=false,MPI_Comm comm=0,int sent = 0);
  Bool_t R__NeedInitialMerge(TDirectory *dir);
  void RunParallel(bool cache=false,MPI_Comm comm=0,int sent=0);
  void MPIWrite(bool cache=false);
  Int_t GetGlobalRank();
 ClassDef(TMPIFile,0)
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
   Bool_t NeedMerge(Float_t clientThreshold);
   Bool_t NeedFinalMerge();
   void RegisterClient(UInt_t clientID,TFile *file);

   TClientInfo tcl;
   // TClientInfo ntcl(char *filename,UInt_t clientID);

 }; 
  MPI_Comm SplitMPIComm(MPI_Comm source,int comm_no);
  int fColor;
  void GetRootName();

};
#endif
