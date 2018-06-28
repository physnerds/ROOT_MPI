/// \file
/// \ingroup tutorial_net
/// This script shows how to make a simple iterative server that
/// can accept connections while handling currently open connections.
/// Compare this script to hserv.C that blocks on accept.
/// In this script a server socket is created and added to a monitor.
/// A monitor object is used to monitor connection requests on
/// the server socket. After accepting the connection
/// the new socket is added to the monitor and immediately ready
/// for use. Once two connections are accepted the server socket
/// is removed from the monitor and closed. The monitor continues
/// monitoring the sockets.
///
/// To run this demo do the following:
///   - Open three windows
///   - Start ROOT in all three windows
///   - Execute in the first window: .x hserv2.C
///   - Execute in the second and third windows: .x hclient.C
///
/// \macro_code
///
/// \author Fons Rademakers

#include "TMessage.h"
#include "TBenchmark.h"
#include "TSocket.h"
#include "TH2.h"
#include "TTree.h"
#include "TMemFile.h"
#include "TRandom.h"
#include "TRandom3.h"
#include "TError.h"
#include "TFileMerger.h"

#include "TServerSocket.h"
#include "TPad.h"
#include "TCanvas.h"
#include "TMonitor.h"

#include "TFileCacheWrite.h"
#include "TSystem.h"
#include "THashTable.h"

#include "TMath.h"
#include "TTimeStamp.h"

#include "TList.h"
#include "mpi.h"

const int kIncremental = 0;
const int kReplaceImmediately = 1;
const int kReplaceWait = 2;

#include "TKey.h"
static Bool_t R__NeedInitialMerge(TDirectory *dir)
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

static void R__DeleteObject(TDirectory *dir, Bool_t withReset)
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

static void R__MigrateKey(TDirectory *destination, TDirectory *source)
{
if (destination==0 || source==0) return;
printf("R__MigrateKey::Trying to migrate the file\n ");
TIter nextkey(source->GetListOfKeys());
printf("R__MigrateKey::Trying to get the list of keys\n");
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

struct ClientInfo
{
   TFile      *fFile;      // This object does *not* own the file, it will be own by the owner of the ClientInfo.
   TString    fLocalName;
   UInt_t     fContactsCount;
   TTimeStamp fLastContact;
   Double_t   fTimeSincePrevContact;

   ClientInfo() : fFile(0), fLocalName(), fContactsCount(0), fTimeSincePrevContact(0) {}
   ClientInfo(const char *filename, UInt_t clientId) : fFile(0), fContactsCount(0), fTimeSincePrevContact(0) {
      fLocalName.Form("%s-%d-%d",filename,clientId,gSystem->GetPid());
   }

   void Set(TFile *file)
   {
      // Register the new file as coming from this client.
      if (file != fFile) {
	printf("Set::new file incoming\n");
         // We need to keep any of the keys from the previous file that
         // are not in the new file.
         if (fFile) {	 
	   printf("Set::File already exists...migrating keys\n");
            R__MigrateKey(fFile,file);
            // delete the previous memory file (if any)
            delete file;
         } else {
            fFile = file;
         }
      }
      TTimeStamp now;
      fTimeSincePrevContact = now.AsDouble() - fLastContact.AsDouble();
      fLastContact = now;
      ++fContactsCount;
   }
};

struct ParallelFileMerger : public TObject
{
  typedef std::vector<ClientInfo> ClientColl_t;

   TString       fFilename;
   TBits         fClientsContact;       //
   UInt_t        fNClientsContact;      //
   ClientColl_t  fClients;
   TTimeStamp    fLastMerge;
   TFileMerger   fMerger;

  ParallelFileMerger(const char *filename, Bool_t writeCache = kFALSE) : fFilename(filename), fNClientsContact(0), fMerger(kFALSE,kTRUE)
   {
      // Default constructor.

      fMerger.SetPrintLevel(0);
      fMerger.OutputFile(filename,"RECREATE");
       if (writeCache) new TFileCacheWrite(fMerger.GetOutputFile(),32*1024*1024);
   }
  ~ParallelFileMerger() 
   {
      // Destructor.

      for(unsigned int f = 0 ; f < fClients.size(); ++f) {
         fprintf(stderr,"Client %d reported %u times\n",f,fClients[f].fContactsCount);
      }
      for( ClientColl_t::iterator iter = fClients.begin();
          iter != fClients.end();
          ++iter)
      {
         delete iter->fFile;
      }
   }

   ULong_t  Hash() const
   {
      // Return hash value for this object.
      return fFilename.Hash();
   }

   const char *GetName() const
   {
      // Return the name of the object which is the name of the output file.
      return fFilename;
   }

   Bool_t InitialMerge(TFile *input)
   {
      // Initial merge of the input to copy the resetable object (TTree) into the output
      // and remove them from the input file.

      fMerger.AddFile(input);

      Bool_t result = fMerger.PartialMerge(TFileMerger::kIncremental | TFileMerger::kResetable);

      R__DeleteObject(input,kTRUE);
      return result;
   }

   Bool_t Merge()
   {
      // Merge the current inputs into the output file.

      R__DeleteObject(fMerger.GetOutputFile(),kFALSE); // Remove object that can *not* be incrementally merge and will *not* be reset by the client code.
      printf("Merge::size of fClients: %d\n",fClients.size());
      for(unsigned int f = 0 ; f < fClients.size(); ++f) {
	printf("Merge::Merging files %d\n",f);
         fMerger.AddFile(fClients[f].fFile);
      }
      Bool_t result = fMerger.PartialMerge(TFileMerger::kAllIncremental);

      // Remove any 'resetable' object (like TTree) from the input file so that they will not
      // be re-merged.  Keep only the object that always need to be re-merged (Histograms).
      for(unsigned int f = 0 ; f < fClients.size(); ++f) {
         if (fClients[f].fFile) {
            R__DeleteObject(fClients[f].fFile,kTRUE);
         } else {
            // We back up the file (probably due to memory constraint)
            TFile *file = TFile::Open(fClients[f].fLocalName,"UPDATE");
            R__DeleteObject(file,kTRUE); // Remove object that can be incrementally merge and will be reset by the client code.
            file->Write();
            delete file;
         }
      }
      fLastMerge = TTimeStamp();
      fNClientsContact = 0;
      fClientsContact.Clear();

      return result;
   }

   Bool_t NeedFinalMerge()
   {
      // Return true, if there is any data that has not been merged.

      return fClientsContact.CountBits() > 0;
   }

   Bool_t NeedMerge(Float_t clientThreshold)
   {
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

   void RegisterClient(UInt_t clientId, TFile *file)
   {
      // Register that a client has sent a file.

     ++fNClientsContact;
      fClientsContact.SetBitNumber(clientId);
      printf("RegisterClient: clientID %d\n",clientId);
						  
      if (fClients.size() < clientId+1) {
						  
         fClients.push_back( ClientInfo(fFilename,clientId) );
      }
      fClients[clientId].Set(file);
   }

  // ClassDef(ParallelFileMerger,0);
};

void mpiparallelMergeServer(bool cache = false,int argc=0,MPI_Comm comm) {
  int rank, world_size;
  MPI_Comm_rank(comm,&rank);
  MPI_Comm_size(comm,&world_size);
  UInt_t clientCount = world_size;
  if(rank!=0){
    char filename[100];
    sprintf(filename,"tempfile_sending.root");
    TMemFile file(filename,"RECREATE");
    TRandom *rand = new TRandom();
    UInt_t idx = rand->Integer(100);
    Float_t px,py;
    TTree *tree = new TTree("tree","tree");
    tree->SetAutoFlush(4000000);
    tree->Branch("px",&px);
    tree->Branch("py",&py);
    
    gRandom->SetSeed();
    for(int i = 0;i<2500;i++){
      gRandom->Rannor(px,py);
      tree->Fill();
    }
    file.Write();
    int count;
    count = file.GetSize();
    char *buf = new char[count];
    file.CopyTo(buf,count);
    printf("Buffer of size %d written in rank %d\n",count,rank);
    MPI_Send(buf,count,MPI_CHAR,0,0,MPI_COMM_WORLD);
    printf("Message from rank %d sent\n",rank);
  }
  else if(rank==0){
    printf("now in rank %d\n",rank);
    int tot_bytes=0;
    THashTable mergers;
    for(int i = 1;i<world_size;i++){
      UInt_t clientIndex = i;
      int count;
      char* buf;
      MPI_Status status;
      MPI_Probe(i,0,MPI_COMM_WORLD,&status);
      MPI_Get_count(&status,MPI_CHAR,&count);
      printf("count of incoming message %d\n",count);
      int source;
      source = status.MPI_SOURCE;
      Int_t client_Id = source-1;
      
      if(count<0){
	printf("Message not received from source %d\n",source);
	//probably no need to end the program...
	// MPI_Finalize();
      }
      if(count>0){
	int number_bytes;
	number_bytes = sizeof(char)*count;
	buf = new char[number_bytes];
	printf("Trying to receive the message here\n");
	MPI_Recv(buf,number_bytes,MPI_CHAR,i,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
	printf("received the message now %d\n",i);
	char incoming[100];
	sprintf(incoming,"tempfile_coming.root");
	tot_bytes += number_bytes; //this should take care of memory update
	TMemFile *infile = new TMemFile(incoming,buf,number_bytes,"UPDATE");
	Long64_t _filesize = infile->GetSize();
	printf("size of the current transient file is %d\n",_filesize);
	printf("transient file updated....\n");
	const Float_t clientThreshold= 0.75;
	ParallelFileMerger *info = (ParallelFileMerger*)mergers.FindObject(incoming);
	if(!info){
	  printf("no file created yet %d\n",i);
	  info = new ParallelFileMerger(incoming,cache);
	  mergers.Add(info);
	}
	  if(R__NeedInitialMerge(infile)){
	  printf("trying to merge the file....\n");
	  info->InitialMerge(infile);
	  printf("did I merge the file yet...\n");
	  }
	info->RegisterClient(client_Id,infile);
	if(info->NeedMerge(clientThreshold)){
	    printf("Mergin from client %d\n",client_Id);
	     info->Merge();
	}
	infile=0;
	}
      TIter next(&mergers);
      ParallelFileMerger *info;
      while ((info = (ParallelFileMerger*)next())){
	printf("now doing final Merge....\n");
	if(info->NeedFinalMerge()){
	  info->Merge();
	}
      }
      }
    mergers.Delete();
      }
  }
//Here the creating part is only done once...merging done in multiple places..
void CreateBufferAndSend(bool cache=false,int argc=0,MPI_Comm comm){
  int rank,size;
  MPI_Comm_rank(comm,&rank);
  MPI_Comm_size(comm,&size);
  if(rank==0)return;
  char filename[100];
  sprintf(filename,"split_temp.root");
  TMemFile file(filename,"RECREATE");
  TRandom *rand = new TRandom();
  Float_t px,py;
  TTree *tree = new TTree("tree","tree");
  tree->SetAutoFlux(4000000);
  tree->Branch("px",&px);
  tree->Branch("py",&py);
  
  gRandom->SetSeed();
  for(int i=0;i<2500;i++){
    gRandom->Rannor(px,py);
    tree->Fill();
  }
  file.Write();
  int count = file.GetSize();
  char *buff = new char[count];
  file.CopyTo(buff,count);
  MPI_Send(buff,count,MPI_CHAR,0,0,comm);
}      
void ReceiveAndMerge(bool cache=false,int argc=0;MPI_Comm comm){
  int rank,size;
  MPI_Comm_rank(comm,&rank);
  MPI_Comm_size(comm,&size);
  if(rank!=0)return;
  THashTable mergers;
  char incoming[100];
  sprintf(incoming,"transient_incoming.root");
  for(int i =1;i<size;i++){
    UInt_t clientIndex=i;
    int count;
    char *buf;
    MPI_Status status;
    MPI_Probe(i,0,MPI_COMM_WORLD,&status);
    MPI_Get_count(&status,MPI_CHAR,&count);
    int source = status.MPI_SOURCE;
    Int_t client_Id = source-1;
    if(count<0)return;
    
    int number_bytes;
    number_bytes = sizeof(char)*count;
    buf = new char[number_bytes];
    MPI_Recv(buf,number_bytes,MPI_CHAR,i,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE); 

    TMemFile *infile = new TMemFile(incoming,buf,number_bytes,"UPDATE");
    const Float_t clientThrehold = 0.75;
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
  }
  TIter next(&mergers);
  ParallelFileMerger *info;
  while((info = (ParallelFileMerger*)next())){
      if(info->NeedFinalMerge()){
	info->Merge();
      }
  }
  if(i==size-1){
    TFile *final_file;
    final_file = new TFile(incoming,"READONLY");
    Long64_t final_count = final_file->GetSize();
    char *final_buf = new char[final_count];
    final_file->WriteBuffer(final_buf,final_cout);
    // SendBuffer(final_buff,final_count,another_comm);  
    delete final_file;
    delete final_buf;
  }
  mergers.Delete();
  
  //try to implement the sending thing...
}
void SendBuffer(char *buff,int buff_size,int color,MPI_Comm comm){
  int comm_size;
  MPI_Comm_size(comm,&comm_size);
  // allocate zero as the sender
  for(int i =1;i<comm_size;i++)MPI_Send(buff,buff_size,0,color,comm);
   
}
MPI_Comm SplitMPIComm(MPI_Comm source){
  int source_rank;
  MPI_Comm row_comm;
  MPI_Comm_rank(source,&source_rank);
  int color = source_rank/2;
  MPI_Comm_split(source,color,source_rank,&row_comm);
  return row_comm;
}

void GetMergedROOTFile(TMemFile* file,TMemFile* fFile){
  file = fFile;
}

int main(int argc,char** argv){
  MPI_Init(&argc,&argv);
  mpiparallelMergeServer(false,0,MPI_COMM_WORLD);
  MPI_Finalize();
  return 0;
}	  
    
	     
       
    

	    
