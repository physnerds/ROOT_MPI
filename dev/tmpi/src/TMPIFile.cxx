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
#include "TClass.h"
#include "TVirtualMutex.h"
#include "TMPIFile.h"
#include "TFileCacheWrite.h"
#include "mpi.h"
#include "TKey.h"
#include "TSystem.h"
#include "TMath.h"
/*
Will basically add all the headers from TMemFile.cxx File later...
 */

//Debug mode implementation later.....
//...
ClassImp(TMPIFile);
//the constructor should be similar to TMemFile...

TMPIFile::TMPIFile(const char *name, char *buffer, Long64_t size,
		   Option_t *option,const char *ftitle,Int_t compress,
		   Int_t np,Int_t split):TMemFile(name,buffer,size,option,ftitle,compress){
  if(buffer)printf("buffer of non 0 size received\n");
  //Initialize MPI if it is not already initialized...
  int flag;
  MPI_Initialized(&flag);
  if(!flag) MPI_Init(&argc,&argv); 
}
TMPIFile::TMPIFile(const char *name,Option_t *option,const char *ftitle,Int_t compress, Int_t np,Int_t split):TMemFile(name,option,ftitle,compress){
  int flag;
  MPI_Initialized(&flag);
  if(!flag)MPI_Init(&argc,&argv);
}
TMPIFile::~TMPIFile(){
  MPI_Finalize();
}
void TMPIFile::PurgeEveryThing(){
  MPI_Finalize();
}
//defining the ParallelFileMerger here.....
//constructor for ParallelFileMerger
TMPIFile::ParallelFileMerger::ParallelFileMerger(const char *filename,Bool_t writeCache):fFilename(filename),fClientsContact(0),fMerger(kFALSE,kTRUE)
{
  fMerger.SetPrintLevel(0);
  fMerger.OutputFile(filename,"RECREATE");
  if(writeCache)new TFileCacheWrite(fMerger.GetOutputFile(),32*1024*1024);

}
//And the destructor....
TMPIFile::ParallelFileMerger::~ParallelFileMerger()
{
  for(ClientColl_t::iterator iter = fClients.begin();
      iter != fClients.end();++iter)delete iter->fFile;

}
ULong_t TMPIFile::ParallelFileMerger::Hash()const{
  return fFilename.Hash();
}
const char *TMPIFile::ParallelFileMerger::GetName()const{
  return fFilename;
}
Bool_t TMPIFile::ParallelFileMerger::InitialMerge(TFile *input)
{
      // Initial merge of the input to copy the resetable object (TTree) into the output
      // and remove them from the input file.
  fMerger.AddFile(input);
  Bool_t result = fMerger.PartialMerge(TFileMerger::kIncremental | TFileMerger::kResetable);
  tcl.R__DeleteObject(input,kTRUE);
  return result;
}
Bool_t TMPIFile::ParallelFileMerger::Merge()
{
  tcl.R__DeleteObject(fMerger.GetOutputFile(),kFALSE); //removing object that cannot be incrementally merged and will not be reset by the client code..
  for(unsigned int f = 0; f<fClients.size();++f){
    fMerger.AddFile(fClients[f].fFile);
  }
  Bool_t result = fMerger.PartialMerge(TFileMerger::kAllIncremental);
  // Remove any 'resetable' object (like TTree) from the input file so that they will not
  // be re-merged.  Keep only the object that always need to be re-merged (Histograms).
  for(unsigned int f = 0 ; f < fClients.size(); ++f) {
    if (fClients[f].fFile) {
      tcl.R__DeleteObject(fClients[f].fFile,kTRUE);
    } else {
      // We back up the file (probably due to memory constraint)
      TFile *file = TFile::Open(fClients[f].fLocalName,"UPDATE");
      tcl.R__DeleteObject(file,kTRUE); // Remove object that can be incrementally merge and will be reset by the client code.
      file->Write();
      delete file;
    }
  }
  fLastMerge = TTimeStamp();
  fNClientsContact = 0;
  fClientsContact.Clear();

  return result;
}
void TMPIFile::ParallelFileMerger::RegisterClient(UInt_t clientID,TFile *file){
  // Register that a client has sent a file.

  ++fNClientsContact;
  fClientsContact.SetBitNumber(clientID);
  printf("RegisterClient: clientID %d\n",clientID);
  TClientInfo ntcl(std::string(fFilename).c_str(),clientID);					  
  if (fClients.size() < clientID+1) {

    // fClients.push_back(TClientInfo(std::string(fFilename).c_str(),clientID) );
     fClients.push_back(ntcl);
  }
  fClients[clientID].Set(file);
}

Bool_t TMPIFile::ParallelFileMerger::NeedMerge(Float_t clientThreshold){
  // Return true, if enough client have reported

  if (fClients.size()==0) {
    return kFALSE;
  }

  // Calculate average and rms of the time between the last 2 contacts.
  Double_t sum = 0;
  Double_t sum2 = 0;
  printf("NeedMerge:: Size of fClients: %d\n",fClients.size());
  for(unsigned int c = 0 ; c < fClients.size(); ++c) {
    sum += fClients[c].fTimeSincePrevContact;
    sum2 += fClients[c].fTimeSincePrevContact*fClients[c].fTimeSincePrevContact;
  }
  Double_t avg = sum / fClients.size();
  Double_t sigma = sum2 ? TMath::Sqrt( sum2 / fClients.size() - avg*avg) : 0;
  Double_t target = avg + 2*sigma;
  TTimeStamp now;
  if ( (now.AsDouble() - fLastMerge.AsDouble()) > target) {
    //         Float_t cut = clientThreshold * fClients.size();
    //         if (!(fClientsContact.CountBits() > cut )) {
    //            for(unsigned int c = 0 ; c < fClients.size(); ++c) {
    //               fprintf(stderr,"%d:%f ",c,fClients[c].fTimeSincePrevContact);
    //            }
    //            fprintf(stderr,"merge:%f avg:%f target:%f\n",(now.AsDouble() - fLastMerge.AsDouble()),avg,target);
    //         }
    return kTRUE;
  }
  Float_t cut = clientThreshold * fClients.size();
  return fClientsContact.CountBits() > cut  || fNClientsContact > 2*cut;

}
Bool_t TMPIFile::ParallelFileMerger::NeedFinalMerge()
{
  return fClientsContact.CountBits()>0;
}
 void TMPIFile::R__MigrateKey(TDirectory *destination, TDirectory *source)
{
if (destination==0 || source==0) return;
TIter nextkey(source->GetListOfKeys());
   TKey *key;
   while( (key = (TKey*)nextkey()) ) {
      TClass *cl = TClass::GetClass(key->GetClassName());
// printf("R__MigrateKey::Class name so far....%s\n",key->GetClassName());
      if (cl->InheritsFrom(TDirectory::Class())) {
         TDirectory *source_subdir = (TDirectory *)source->GetList()->FindObject(key->GetName());
         if (!source_subdir) {
            source_subdir = (TDirectory *)key->ReadObj();
         }
         TDirectory *destination_subdir = destination->GetDirectory(key->GetName());
         if (!destination_subdir) {
            destination_subdir = destination->mkdir(key->GetName());
         }
         R__MigrateKey(destination,source);
      } else {
         TKey *oldkey = destination->GetKey(key->GetName());
         if (oldkey) {
            oldkey->Delete();
            delete oldkey;
         }
         TKey *newkey = new TKey(destination,*key,0 /* pidoffset */); // a priori the file are from the same client ..
         destination->GetFile()->SumBuffer(newkey->GetObjlen());
         newkey->WriteFile(0);
         if (destination->GetFile()->TestBit(TFile::kWriteError)) {
            return;
         }
      }
   }
   destination->SaveSelf();
}


void TMPIFile::R__DeleteObject(TDirectory *dir, Bool_t withReset)
{
   if (dir==0) return;

 TIter nextkey(dir->GetListOfKeys());
   TKey *key;
   while( (key = (TKey*)nextkey()) ) {
      TClass *cl = TClass::GetClass(key->GetClassName());
      if (cl->InheritsFrom(TDirectory::Class())) {
         TDirectory *subdir = (TDirectory *)dir->GetList()->FindObject(key->GetName());
         if (!subdir) {
            subdir = (TDirectory *)key->ReadObj();
         }
         R__DeleteObject(subdir,withReset);
      } else {
         Bool_t todelete = kFALSE;
         if (withReset) {
            todelete = (0 != cl->GetResetAfterMerge());
         } else {
            todelete = (0 ==  cl->GetResetAfterMerge());
         }
         if (todelete) {
            key->Delete();
            dir->GetListOfKeys()->Remove(key);
            delete key;
         }
      }
   }
}

MPI_Comm TMPIFile::SplitMPIComm(MPI_Comm source, int comm_no){
  int source_rank,source_size;
  MPI_Comm row_comm;
  MPI_Comm_rank(source,&source_rank);
  MPI_Comm_size(source,&source_size);
  if(comm_no>source_size){
    SysError("TMPIFile","number of sub communicators larger than mother size");
    exit(1);
  }
  int color = source_rank/comm_no;
  MPI_Comm_split(source,color,source_rank,&row_comm);
  return row_comm;
}

void TMPIFile::SendBuffer(char *buff, int buff_size,int color, MPI_Comm comm){
  int comm_size,comm_rank;
  MPI_Comm_size(comm,&comm_size);
  MPI_Comm_rank(comm,&comm_rank);
  
  if(comm_rank!=0)MPI_Send(buff,buff_size,MPI_CHAR,0,color,comm);
  else return;
}
void TMPIFile::ReceiveAndMerge(bool cache,int argc,MPI_Comm comm,int color,int rank,int size){
  MPI_Comm_rank(comm,&rank);
  MPI_Comm_size(comm,&size);
  if(rank!=0)return;
  printf("Now in ReceiveAndMerge %d %d %d\n",rank,size,color);
  THashTable mergers;
  char incoming[100];
  sprintf(incoming,"transient_incoming%d.root",color);
  for(int i =1;i<size;i++){
    UInt_t clientIndex=i;
    int count;
    char *buf;
    MPI_Status status;
    MPI_Probe(i,color,comm,&status);
    MPI_Get_count(&status,MPI_CHAR,&count);
    printf("Status Gathered\n");
    int source = status.MPI_SOURCE;
    Int_t client_Id = source-1;
    if(count<0)return;
    
    int number_bytes;
    number_bytes = sizeof(char)*count;
    buf = new char[number_bytes];
    MPI_Recv(buf,number_bytes,MPI_CHAR,i,color,comm,MPI_STATUS_IGNORE); 

    TMemFile *infile = new TMemFile(incoming,buf,number_bytes,"UPDATE");
    const Float_t clientThreshold = 0.75;
    ParallelFileMerger *info = (ParallelFileMerger*)mergers.FindObject(incoming);
    if(!info){
      info = new ParallelFileMerger(incoming,cache);
      mergers.Add(info);
    }
    if(R__NeedInitialMerge(infile)){
      // printf("trying to merge the file....\n");
      info->InitialMerge(infile);
      //printf("did I merge the file yet...\n");
    }
    info->RegisterClient(client_Id,infile);
    if(info->NeedMerge(clientThreshold)){
      //printf("Mergin from client %d\n",client_Id);
      info->Merge();
    }
    infile=0;
  
    TIter next(&mergers);
    while((info = (ParallelFileMerger*)next())){
      if(info->NeedFinalMerge()){
	info->Merge();
      }
    }
    /* if(i==size-1){
       TFile *final_file;
       final_file = new TFile(incoming,"READONLY");
       Long64_t final_count = final_file->GetSize();
       char *final_buf = new char[final_count];
       // final_file->WriteBuffer(final_buf,final_count);
       // SendBuffer(final_buff,final_count,another_comm);  
       delete final_file;
       delete final_buf;
       }*/
  }
  mergers.Delete();
  //MPI_Comm_free(&comm);
  
  //try to implement the sending thing...
}
//here I want to have creating buffer and Sending...probably this is also the function which
//should be modified according to the data that has to be read/processed and written....
Bool_t TMPIFile::R__NeedInitialMerge(TDirectory *dir)
{
  if (dir==0) return kFALSE;

  TIter nextkey(dir->GetListOfKeys());
  TKey *key;
  while( (key = (TKey*)nextkey()) ) {
    TClass *cl = TClass::GetClass(key->GetClassName());
    if (cl->InheritsFrom(TDirectory::Class())) {
      TDirectory *subdir = (TDirectory *)dir->GetList()->FindObject(key->GetName());
      if (!subdir) {
	subdir = (TDirectory *)key->ReadObj();
      }
      if (R__NeedInitialMerge(subdir)) {
	return kTRUE;
      }
    } else {
      if (0 != cl->GetResetAfterMerge()) {
	return kTRUE;
      }
    }
  }
  return kFALSE;
}

void TMPIFile::CreateBufferAndSend(TMemFile *file,bool cache,MPI_Comm comm,int color,int sent)
{
  int rank,size;
  MPI_Comm_rank(comm,&rank);
  MPI_Comm_size(comm,&size);
  if(rank==0)return;
  int count = file->GetSize();
  char *buff = new char[count];
  file->CopyTo(buff,count);
  sent = MPI_Send(buff,count,MPI_CHAR,0,color,comm);
  delete file;
}

