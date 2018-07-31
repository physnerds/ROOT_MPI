
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
#include "TTree.h"
#include <string>
/*
Will basically add all the headers from TMemFile.cxx File later...
 */

//Debug mode implementation later.....
//...
ClassImp(TMPIFile);
//the constructor should be similar to TMemFile...

TMPIFile::TMPIFile(const char *name, char *buffer, Long64_t size,
		   Option_t *option,Int_t split,Int_t sync_rate,const char *ftitle,
		   Int_t compress):TMemFile(name,buffer,size,option,ftitle,compress),fColor(0),frequest(0),fSplitLevel(split),fSyncRate(sync_rate){
  if(buffer)printf("buffer of non 0 size received\n");
  //Initialize MPI if it is not already initialized...
  int flag;
  MPI_Initialized(&flag);
  if(!flag) MPI_Init(&argc,&argv); 
  int global_size,global_rank;
  MPI_Comm_size(MPI_COMM_WORLD,&global_size);
  MPI_Comm_rank(MPI_COMM_WORLD,&global_rank);
  if(split!=0){
    if(2*split>global_size){
      SysError("TMPIFile"," Number of Output File is larger than number of Processors Allocated. Number of processors should be two times larger than outpts. For %d outputs at least %d should be allocated instead of %d\n.",split,2*split,global_size);
      exit(1);
    }
    int tot = global_size/split;
    if(global_size%split!=0){
      int n = global_size%split;
      tot=tot+1;
      fColor = global_rank/tot;
      row_comm = SplitMPIComm(MPI_COMM_WORLD,tot);
    }
    else{
      fColor = global_rank/tot;
      row_comm = SplitMPIComm(MPI_COMM_WORLD,tot);
    }
  }
  else{
    fColor = 0;
    // printf("TMPIFile::TMPIFile no split \n");
    row_comm = MPI_COMM_WORLD;
  }
}
TMPIFile::TMPIFile(const char *name,Option_t *option,Int_t split,Int_t sync_rate,const char *ftitle,
		   Int_t compress):TMemFile(name,option,ftitle,compress),fColor(0),frequest(0),fSplitLevel(split),fSyncRate(sync_rate){
  int flag;
  MPI_Initialized(&flag);
  if(!flag)MPI_Init(&argc,&argv);
  int global_size,global_rank;
  MPI_Comm_size(MPI_COMM_WORLD,&global_size);
  MPI_Comm_rank(MPI_COMM_WORLD,&global_rank);
  if(split!=0){
    if(2*split>global_size){
      SysError("TMPIFile"," Number of Output File is larger than number of Processors Allocated. Number of processors should be two times larger than outpts. For %d outputs at least %d should be allocated instead of %d\n.",split,2*split,global_size);
      exit(1);
    }
    int tot = global_size/split;
    if(global_size%split!=0){
      int n = global_size%split;
      tot=tot+1;
      fColor = global_rank/tot;
      row_comm = SplitMPIComm(MPI_COMM_WORLD,tot);
    }
    else{
      fColor = global_rank/tot;
      row_comm = SplitMPIComm(MPI_COMM_WORLD,tot);
    }
  }
  else{
    fColor = 0;
    // printf("TMPIFile::TMPIFile no split \n");
    row_comm = MPI_COMM_WORLD;
  }
}
TMPIFile::~TMPIFile(){
  //  MPI_Finalize();
  Close();
  //TRACE("destroy");
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
  // printf("NEed final merge\n");
  return fClientsContact.CountBits()>0;
}
 void TMPIFile::R__MigrateKey(TDirectory *destination, TDirectory *source)
{
if (destination==0 || source==0) return;
TIter nextkey(source->GetListOfKeys());
   TKey *key;
   printf("Trying to Migrat the keys merge\n");
   while( (key = (TKey*)nextkey()) ) {
      TClass *cl = TClass::GetClass(key->GetClassName());
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

void TMPIFile::SendBuffer(char *buff, int buff_size, MPI_Comm comm){
  int comm_size,comm_rank;
  MPI_Comm_size(comm,&comm_size);
  MPI_Comm_rank(comm,&comm_rank);
  MPI_Request request;
  if(comm_rank!=0){
    MPI_Isend(buff,buff_size,MPI_CHAR,0,fColor,comm,&request);
    MPI_Wait(&frequest,MPI_STATUS_IGNORE);
  }
  else return;
}

void TMPIFile::ReceiveBuffer(bool cache,MPI_Comm comm,int rank,int size){
  if(rank!=0)return;
  int count;
  MPI_Status status;
  MPI_Probe(MPI_ANY_SOURCE,MPI_ANY_TAG,comm,&status);
  MPI_Get_count(&status,MPI_CHAR,&count);
  int source = status.MPI_SOURCE;
  int tag = status.MPI_TAG;
  if(count==0){
    char *buf;
    //   printf("UPDATING END PROCESS %d\n",fEndProcess);
    this->UpdateEndProcess();
    MPI_Recv(buf,0,MPI_CHAR,source,tag,comm,MPI_STATUS_IGNORE); 
    delete buf;
  }
  else {
    // printf("Source %d and Count %d\n",source,count);
    this->ReceiveAndMerge(cache,comm,rank,size);}
}
void TMPIFile::ReceiveAndMerge(bool cache,MPI_Comm comm,int rank,int size){
 if(rank!=0)return;
 // char *buf;
  this->GetRootName();
  THashTable mergers;
  // int empty_buff=0;
  int counter=1;
  while(fEndProcess!=size-1){
    char *buf;
    int count;
  MPI_Status status;
  MPI_Probe(MPI_ANY_SOURCE,MPI_ANY_TAG,comm,&status);
  MPI_Get_count(&status,MPI_CHAR,&count);
  int number_bytes = sizeof(char)*count;
  buf = new char[number_bytes];
  int source = status.MPI_SOURCE;
  int tag = status.MPI_TAG;
  if(number_bytes==0){
    //  printf("UPDATING END PROCESS %d %d\n",fEndProcess,source);
    this->UpdateEndProcess();
    //   printf("PROCESS UPDATED %d %d\n",fEndProcess,source);
     MPI_Recv(buf,number_bytes,MPI_CHAR,source,tag,comm,MPI_STATUS_IGNORE); 
     //  delete [] buf;
  }
  else{
  // printf("fEndProcess %d size-1 %d source %d\n",fEndProcess,size-1,source);

    //  printf("Total counts from rank %d color %d  %d\n",source,fColor,count);
    MPI_Recv(buf,number_bytes,MPI_CHAR,source,tag,comm,MPI_STATUS_IGNORE); 
    // printf("ReceiveAndMerge:: From Worker Rank %d\n",source);    
      Int_t client_Id =counter-1;
      //  int number_bytes;
      // number_bytes = sizeof(char)*count;
      // buf = new char[number_bytes];
      // MPI_Recv(buf,number_bytes,MPI_CHAR,source,tag,comm,MPI_STATUS_IGNORE); 
    TMemFile *infile = new TMemFile(fMPIFilename,buf,number_bytes,"UPDATE"); 
    //  printf("Counter is %d\n",client_Id);
    const Float_t clientThreshold = 0.75;
    ParallelFileMerger *info = (ParallelFileMerger*)mergers.FindObject(fMPIFilename);
    if(!info){
      // printf("not info yet...\n");
      info = new ParallelFileMerger(fMPIFilename,cache);
      mergers.Add(info);
    }
    if(R__NeedInitialMerge(infile)){
      //    printf("TMPIFile::ReceiveAndMerge::trying to merge the file....\n");
      info->InitialMerge(infile);
      //   printf("TMPIFile::ReceiveAndMerge::did I merge the file yet...\n");
    }
    info->RegisterClient(client_Id,infile);
    if(info->NeedMerge(clientThreshold)){
      //    printf("TMPIFile::ReceiveAndMErge::Merging from client %d\n",client_Id);
      info->Merge();
    }
    infile = 0;
  
    TIter next(&mergers);
    while((info = (ParallelFileMerger*)next())){
      if(info->NeedFinalMerge()){
	info->Merge();
      }
    }
    counter=counter+1;
    // delete [] buf; UNCOMMENTING THIS GAVE DOUBLE LINKED ERROR WHICH MIGHT MEAN THAT PERHAPS
    //MEMORY IS ALREADY CLEARED.....
  }
  // delete [] buf;
  }
  // mergers.Delete();
  if(fEndProcess==size-1){
    mergers.Delete();
    // delete buf;
    //  printf("Time to exit the function\n");
    return;
  }
  }
/*
void TMPIFile::ReceiveAndMerge(bool cache,MPI_Comm comm,int rank,int size,int source,int tag,int count){
  if(source==-1){
    printf("Undefined Source...\n");
    exit(1);
  }
  THashTable mergers;
  MPI_Status status;
  this->GetRootName();
  // int empty_buff=0;
  int counter=1;
  while(fEndProcess!=size-1){
    printf("fEndProcess %d size-1 %d\n",fEndProcess,size-1);
    int count;
    char *buf;
    int number_bytes;
    number_bytes = sizeof(char)*count;
    buf = new char[number_bytes];
     printf("Total counts from rank %d color %d  %d\n",source,fColor,count);
    MPI_Recv(buf,number_bytes,MPI_CHAR,source,tag,comm,MPI_STATUS_IGNORE); 
    // printf("ReceiveAndMerge:: From Worker Rank %d\n",source);    
      Int_t client_Id =counter-1;
      //  int number_bytes;
      // number_bytes = sizeof(char)*count;
      // buf = new char[number_bytes];
      // MPI_Recv(buf,number_bytes,MPI_CHAR,source,tag,comm,MPI_STATUS_IGNORE); 
    TMemFile *infile = new TMemFile(fMPIFilename,buf,number_bytes,"UPDATE"); 
    //  printf("Counter is %d\n",client_Id);
    const Float_t clientThreshold = 0.75;
    ParallelFileMerger *info = (ParallelFileMerger*)mergers.FindObject(fMPIFilename);
    if(!info){
      // printf("not info yet...\n");
      info = new ParallelFileMerger(fMPIFilename,cache);
      mergers.Add(info);
    }
    if(R__NeedInitialMerge(infile)){
      //    printf("TMPIFile::ReceiveAndMerge::trying to merge the file....\n");
      info->InitialMerge(infile);
      //   printf("TMPIFile::ReceiveAndMerge::did I merge the file yet...\n");
    }
    info->RegisterClient(client_Id,infile);
    if(info->NeedMerge(clientThreshold)){
      //    printf("TMPIFile::ReceiveAndMErge::Merging from client %d\n",client_Id);
      info->Merge();
    }
    infile = 0;
  
    TIter next(&mergers);
    while((info = (ParallelFileMerger*)next())){
      if(info->NeedFinalMerge()){
	info->Merge();
      }
    }
    counter=counter+1;
  }
  mergers.Delete();

  }
*/
Bool_t TMPIFile::R__NeedInitialMerge(TDirectory *dir)
{
  if (dir==0) return kFALSE;
  TIter nextkey(dir->GetListOfKeys());
  TKey *key;
  while( (key = (TKey*)nextkey()) ) {
    TClass *cl = TClass::GetClass(key->GetClassName());
    const char *classname = key->GetClassName();
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

void TMPIFile::CreateBufferAndSend(TMemFile *file,bool cache,MPI_Comm comm,int sent)
{
   int rank,size;
    MPI_Comm_rank(comm,&rank);
   MPI_Comm_size(comm,&size);
  if(rank==0)return;
  int count = file->GetSize();
  char *buff = new char[count];
  file->CopyTo(buff,count);
  MPI_Request request;
  sent = MPI_Isend(buff,count,MPI_CHAR,0,fColor,comm,&request);
  delete file;
}
void TMPIFile::CreateBufferAndSend(bool cache,MPI_Comm comm,int sent)
{
  int rank,size;
  const char* _filename = this->GetName();
  this->Write();
  MPI_Comm_rank(comm,&rank);
  MPI_Comm_size(comm,&size);
  if(rank==0)return;
  int count =  this->GetSize();
  char *buff = new char[count];
  this->CopyTo(buff,count); 
  sent = MPI_Send(buff,count,MPI_CHAR,0,fColor,comm);
  //  MPI_Wait(&frequest,MPI_STATUS_IGNORE);
  if(sent)delete [] buff; //cleanup the memory
}
void TMPIFile::CreateEmptyBufferAndSend(bool cache,MPI_Comm comm,int sent)
{
  int rank,size;
  MPI_Comm_rank(comm,&rank);
  MPI_Comm_size(comm,&size);
  if(this->IsCollector())return;
  char* buff;
  sent = MPI_Send(buff,0,MPI_CHAR,0,fColor,comm);
  // printf("Sent Empty Buffer..at barrier now %d\n",rank);
  if(sent)delete [] buff; //memory cleanup
}

void TMPIFile::MPIWrite(bool cache)
{
  //by this time, MPI should be initialized...
  int rank,size,sent;
  MPI_Comm_rank(row_comm,&rank);
  MPI_Comm_size(row_comm,&size);
  CreateBufferAndSend(cache,row_comm,sent);
  ReceiveAndMerge(cache,row_comm,rank,size);
}
void TMPIFile::MPIFinalWrite(bool cache)
{
  //by this time, MPI should be initialized...
  int rank,size,sent;
  MPI_Comm_rank(row_comm,&rank);
  MPI_Comm_size(row_comm,&size);
CreateEmptyBufferAndSend(cache,row_comm,sent);
ReceiveAndMerge(cache,row_comm,rank,size);
}
void TMPIFile::GetRootName()
{
  std::string _filename = this->GetName();
  //printf("name is %s\n",_filename.c_str());
  int found = _filename.rfind(".root");
  if(found != std::string::npos)_filename.resize(found);
  const char* _name = _filename.c_str();
  sprintf(fMPIFilename,"%s_%d.root",_name,fColor);

}

Int_t TMPIFile::GetMPIGlobalRank(){
  int flag;
  int rank;
  MPI_Initialized(&flag);
  if(flag)MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  else{rank=-9999;}
  return rank;
}

Int_t TMPIFile::GetMPILocalRank(){
  int flag,rank;
  MPI_Initialized(&flag);
  if(flag)MPI_Comm_rank(row_comm,&rank);
  else(rank=-9999);
  return rank;
}

Int_t TMPIFile::GetMPIGlobalSize(){
  int flag,size;
  MPI_Initialized(&flag);
  if(flag)MPI_Comm_size(MPI_COMM_WORLD,&size);
  else{size=-1;}
  return size;
}
Int_t TMPIFile::GetMPILocalSize(){
  int flag,size;
  MPI_Initialized(&flag);
  if(flag)MPI_Comm_size(row_comm,&size);
  else{size=-1;}
  return size;
}
Int_t TMPIFile::GetMPIColor(){
  return fColor;
}
Bool_t TMPIFile::IsCollector(){
  Bool_t coll=false;
  int rank=this->GetMPILocalRank();
  if(rank==0)coll=true;
  return coll;
}
Int_t TMPIFile::GetSplitLevel(){
  return fSplitLevel;
}

Int_t TMPIFile::GetSyncRate(){
  return fSyncRate;
}

void TMPIFile::MPIWrite(int entry,int tot_entry,bool cache){
  //if(entry%fSyncRate==0)
  this->MPIWrite(cache);
   if(entry==tot_entry-1){
   this->MPIFinalWrite(cache);
     }
}
void TMPIFile::UpdateEndProcess(){
  fEndProcess=fEndProcess+1;
}
void TMPIFile::RunCollector(bool cache)
{
  //by this time, MPI should be initialized...
  int rank,size;
  MPI_Comm_rank(row_comm,&rank);
  MPI_Comm_size(row_comm,&size);
  ReceiveAndMerge(cache,row_comm,rank,size);
}

void TMPIFile::Sync(bool cache){
  int rank,size,sent;
  MPI_Comm_rank(row_comm,&rank);
  MPI_Comm_size(row_comm,&size);
  CreateBufferAndSend(cache,row_comm,sent);
}
void TMPIFile::MPIClose(bool cache){
    int rank,size,sent;
  MPI_Comm_rank(row_comm,&rank);
  MPI_Comm_size(row_comm,&size);
  CreateEmptyBufferAndSend(cache,row_comm,sent);
  // printf("Now trying to close %d rank\n",rank);
   MPI_Barrier(row_comm);
   //okay once they reach the buffer...just close it.
   this->Close();
  
}
