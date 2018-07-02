//A TMemFile that utilizes MPI Libraries to create and Merge ROOT Files


#ifndef ROOT_TMPIFile
#define ROOT_TMPIFile

#include "TMemFile.h"
#include "TKey.h"
#include "mpi.h"
#include <vector>
#include <memory>

class TMPIFile : public TMemFile {
public:

private:
static Bool_t R__NeedInitialMerge(TDirectory *dir);
static Bool_t R__DeleteObject(TDirectory *dir,Bool_t withReset);
static void R__MigrateKey(TDirectory *destination,TDirectory *source);


struct ClientInfo{
public:
TFile *fFile;
TString fLocalName;
UInt_t fConactsCount;
TTimeStamp fLastContact;
Double_t fTimeSincePrevContact;

ClientInfo():fFile(0).fLocalName(),fContactsCount(0),fTimeSincePrevContact(0){}
ClientInfo(const char *filename, UInt_t clientId) : fFile(0), fContactsCount(0), fTimeSincePrevContact(0) {
      fLocalName.Form("%s-%d-%d",filename,clientId,gSystem->GetPid());
}
  void Set(TFle *file);
};
struct ParallelFileMerger : public TObject{
public:
typedef std::vector<ClientInfo>ClientColl_t;

TString fFilename;
TBits fClientsContact;
UInt_t fNClientsContact;
ClientColl_t fClients;
TTimeStamp fLastMerge;
TFileMerger fMerger;

ParallelFileMerger(const char *filename,Bool_t writeCache=kFALSE);
~ParallelFileMerger();

ULong_t Hash();
const char *GetName();
Bool_t InitialMerge(TFile *input);
Bool_t Merger();
Bool_t NeedFinalMerge(){return fClientsContact.CountBits()>0};
Bool_t NeedMerge(Float_t clientThreshold);
void RegisterClient(UInt_t clientID,TFile *file);

};

void mpiparallelMergerServer(bool_t cache,int argc,MPI_Comm comm,int color);
void CreateBufferAndSend(bool cache,MPI_Comm comm);
void SendBuffer(char *buffer,int buff_size,int color,MPI_Comm comm);
MPI_Comm SplitMPIComm(MPI_Comm source,int comm_no);

public:
//now we define the constructors, destructors and other needed files here...
//constructor similar to TMemFile but some extra arguments...

//later define what to do in constructor and destructor.....
TMPIFile(const char *name,Option_t *option="",const char *ftitle="",Int_t compress=4,Int_t np=2,Int_t split=0);//at least two processors and division of subgroups
virtual ~TMPIFile();

 


};
