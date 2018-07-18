
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

#include "dkmeta.h"
#include "dk2nu.h"
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
#include <ctime>

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
      printf("Name of objects %s\n",key->GetName());
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
//example to handle meta information
static void R__HandleMetaInformation(TFile *outputfile,const char* Meta= "metatree"){
  TTree *meta;
  meta = (TTree*)outputfile->Get(Meta);
  Long64_t nentries = meta->GetEntries();
  TBranch *_pots = (TBranch*)meta->GetBranch("POTS");
  Float_t tot_pots=0.0;
  for(int jentry=0;jentry<nentries;jentry++){
    meta->GetEntry(jentry);
    tot_pots += _pots->GetEntry(jentry);
  }
  //now we want to replace this with 
  //then again rewrite with new info...
  Float_t POTS=tot_pots;
  printf("Total pots is %f\n",tot_pots);
  //meta->Fill();
  TFile *new_file =new TFile  ("blah_di_blah.root"/*outputfile->GetName()*/,"RECREATE");
  TTree *newtree = ((TTree*)outputfile->Get("tree"))->CloneTree();
  //and the meta_tree
  TTree *meta_tree = new TTree("metatree","metatree");
  
  meta_tree->Branch("POTS",&POTS);
  new_file->Write();
  new_file->Close();
  //do we need to delete the pointer here or not....
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
	    // R__HandleMetaInformation(file);
            R__DeleteObject(file,kTRUE); // Remove object that can be incrementally merge and will be reset by the client code.
	    // R__HandleMetaInformation(file);
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

void mpiparallelMergeServer(bool cache = false,int argc=0,MPI_Comm comm=0,int color=0) {
  //declare the functions here....
  void ReceiveAndMerge(bool cache,int argc, MPI_Comm _comm,int _color,int _rank,int _size);
  void CreateBufferAndSend(bool cache,int _argc,MPI_Comm _comm);
  void metaFill(TTree *metatree,MPI_Comm _comm);
  void FillDkMetaTree(bsim::DkMeta* dkmeta);
  void FillDk2NuTree(bsim::Dk2Nu* dk2nu,int Seed);
  int rank, size;
  MPI_Comm_rank(comm,&rank);
  MPI_Comm_size(comm,&size);
  UInt_t clientCount = size;
  int seed  = size+color+rank;
  int sent=0;
  if(rank!=0){
    char filename[100];
    sprintf(filename,"tempfile_sending.root");
    TMemFile file(filename,"RECREATE");
    bsim::Dk2Nu* dk2nu = new bsim::Dk2Nu;
    bsim::DkMeta* dkmeta = new bsim::DkMeta;
    //create the trees.....
    TTree *fDk2nu = new TTree("dk2nuTree","testing with dk2nu struc");
    fDk2nu->Branch("dk2nu","bsim::Dk2Nu",&dk2nu,32000,99);
    //now the dkmeta tree
    TTree *fDkMeta = new TTree("dkmetaTree","testing with meta struc");
    fDkMeta->Branch("dkmeta","bsim::DkMeta",&dkmeta,32000,99);
    FillDkMetaTree(dkmeta);
    FillDk2NuTree(dk2nu,seed);
    fDkMeta->Fill();
    fDk2nu->Fill();
    
    /*
    TRandom *rand = new TRandom();
    UInt_t idx = rand->Integer(100);
    Float_t px,py;
    TTree *tree = new TTree("tree","tree");
    TTree *metatree = new TTree("metatree","metatree");
    tree->SetAutoFlush(4000000);
    tree->Branch("px",&px);
    tree->Branch("py",&py);
    
    Float_t POTS,location[3];
    metatree->Branch("POTS",&POTS);
    metatree->Branch("location",location,"location[3]/F");
    POTS = 2.0E10*Float_t((size-1)); //user need to know provide handling meta data info...
    location[0] =0.0;
    location[1] = 0.0;
    location[2] = 574.00;
    //metaFill(metatree,comm);
    metatree->Fill();
    gRandom->SetSeed();
    for(int i = 0;i<2500;i++){
      gRandom->Rannor(px,py);
      tree->Fill();
    }
    */
    file.Write();
    int count;
    count = file.GetSize();
    char *buf = new char[count];
    file.CopyTo(buf,count);
    printf("Buffer of size %d written in rank %d color %d\n",count,rank,color);
     sent=MPI_Send(buf,count,MPI_CHAR,0,color,comm);
    printf("Message from rank %d sent\n",rank);
  }
  ReceiveAndMerge(cache,argc,comm,color,rank,size);
  }
//Here the creating part is only done once...merging done in multiple places..
void CreateBufferAndSend(bool cache=false,MPI_Comm comm=0){
  int rank,size;
  MPI_Comm_rank(comm,&rank);
  MPI_Comm_size(comm,&size);
  if(rank==0)return;
  char filename[100];
  sprintf(filename,"split_temp.root");
  TMemFile file(filename,"RECREATE");
  //instead try with dk2nu ntuples....

  /*
  TRandom *rand = new TRandom();
  Float_t px,py;
  TTree *tree = new TTree("tree","tree");
  tree->SetAutoFlush(4000000);
  tree->Branch("px",&px);
  tree->Branch("py",&py);
  
  gRandom->SetSeed();
  for(int i=0;i<2500;i++){
    gRandom->Rannor(px,py);
    tree->Fill();
  }
  */
  file.Write();
  int count = file.GetSize();
  char *buff = new char[count];
  file.CopyTo(buff,count);
  MPI_Send(buff,count,MPI_CHAR,0,0,comm);
}      
void ReceiveAndMerge(bool cache=false,int argc=0,MPI_Comm comm=0,int color=0,int rank=0,int size=0){
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
    printf("ReceiveAndMerge::Total Bytes received %d\n",number_bytes);
    MPI_Recv(buf,number_bytes,MPI_CHAR,i,color,comm,MPI_STATUS_IGNORE); 

    TMemFile *infile = new TMemFile(incoming,buf,number_bytes,"UPDATE");
    const Float_t clientThreshold = 0.75;
    ParallelFileMerger *info = (ParallelFileMerger*)mergers.FindObject(incoming);
    if(!info){
      printf("not info yet..../\n");
      info = new ParallelFileMerger(incoming,cache);
      mergers.Add(info);
    }
 	if(R__NeedInitialMerge(infile)){
	   printf("trying to merge the file....\n");
	  info->InitialMerge(infile);
	  //printf("did I merge the file yet...\n");
	}
	info->RegisterClient(client_Id,infile);
	if(info->NeedMerge(clientThreshold)){
	  printf("Mergin from client %d\n",client_Id);
	     info->Merge();
	}
	infile=0;
  
  TIter next(&mergers);
  while((info = (ParallelFileMerger*)next())){
      if(info->NeedFinalMerge()){
	info->Merge();
      }
  }

  }
  mergers.Delete();
  //MPI_Comm_free(&comm);
  TFile *_finalfile = new TFile(incoming,"READONLY");
  // R__HandleMetaInformation(_finalfile);
  //try to implement the sending thing...
}
void SendBuffer(char *buff,int buff_size,int color,MPI_Comm comm){
  int comm_size,comm_rank;
  MPI_Comm_size(comm,&comm_size);
  MPI_Comm_rank(comm,&comm_rank);
  // allocate zero as the sender
  if(comm_rank!=0)MPI_Send(buff,buff_size,MPI_CHAR,0,color,comm);
  else return;
   
}
//Right now implement splitting into two only...
//comm_no also determines total no. of merged outputs here....
MPI_Comm SplitMPIComm(MPI_Comm source,int comm_no){
  void CreateBufferAndSend(bool cache,MPI_Comm comm);
  void mpiparallelMergeServer(bool cache,int argc,MPI_Comm comm,int color);
  int source_rank,source_size;
  MPI_Comm row_comm;
  MPI_Comm_rank(source,&source_rank);
  MPI_Comm_size(source,&source_size);
  if(comm_no>source_size){
    printf("number of sub communicators larger than mother size....Exiting\n");
    exit(1);
  }
  int color = source_rank/comm_no;
  MPI_Comm_split(source,color,source_rank,&row_comm);
  return row_comm;
}

void FillDkMetaTree(bsim::DkMeta* dkmeta){
  dkmeta->pots = 1.0E5;
  dkmeta->beamsim = "mpi_run_test";
  dkmeta->physics = "neutrino decay info mock";
  dkmeta->tgtcfg = "no_tgt";
  dkmeta->physcuts = "sth_sth";
  
  
  dkmeta->location.clear();
  bsim::Location alocation(0.0,0.0,57400.00,"DUNE_NEAR_DET");
  dkmeta->location.push_back(alocation);
}
//now fill the dk2nu information....
void FillDk2NuTree(bsim::Dk2Nu* dk2nu,int seed){
  //here we fill the so called events....
  //clear some stuffs here
  dk2nu->nuray.clear();
  dk2nu->ancestor.clear();
  dk2nu->vint.clear();
  dk2nu->job = seed; //just the rank info
  
  TRandom *rand = new TRandom(seed);
  Float_t px,py,pz,E;
  for(int i=0;i<25000;i++){
    gRandom->Rannor(px,py);
    pz = px*23.3442+py*1.213;
    E = sqrt(px*px+py*py+pz*pz);
    bsim::NuRay Nuinfo(px,py,pz,E,1.0);
    dk2nu->nuray.push_back(Nuinfo);
    for(int j=0;j<4;j++){
      bsim::Ancestor a;
      TRandom *erand = new TRandom(seed+1243);
      Float_t startx,starty,startz;
      gRandom->Rannor(startx,starty);
      a.startx = startx;
      a.starty = starty;
      a.startz = erand->PoissonD(startx);
      a.startpx = erand->PoissonD(a.startz);
      a.startpy = erand->PoissonD(a.startpx);
      a.startpz = erand->PoissonD(a.startpy);
      dk2nu->ancestor.push_back(a);
      delete erand;
    }
  }
  delete rand;
    
}

void GetMergedROOTFile(TMemFile* file,TMemFile* fFile){
  file = fFile;
}

int main(int argc,char** argv){
  std::clock_t start;
  start = std::clock();
  double duration;
  // MPI_Init(&argc,&argv);
  MPI_Init(&argc,&argv);
  int flag;
  MPI_Initialized(&flag);
  if(!flag){
      printf("MPI is not initialized\n");
      MPI_Init(&argc,&argv);
    }
  int source_size;
  MPI_Comm_size(MPI_COMM_WORLD,&source_size);
  const int tot_out = 1;
  int comm_no=source_size/tot_out;
  MPI_Comm row_comm = SplitMPIComm(MPI_COMM_WORLD,comm_no);
  int global_rank,global_size;
  MPI_Comm_rank(MPI_COMM_WORLD,&global_rank);
  MPI_Comm_size(MPI_COMM_WORLD,&global_size);
  int color = global_rank/comm_no;
  mpiparallelMergeServer(false,0,row_comm,color);
  //MPI_Comm_free(&row_comm);
  printf("Finished processing color %d\n",color);
   MPI_Finalize();
   duration = std::clock()-start;
   //printf("Total time to process %d\n",duration);
  return 0;                                                                        
}
	  
    
void metaFill(TTree *metatree,MPI_Comm comm){
  //0 is the collector but we can specify 1 or 2 as the only MPI which sends the meta tree information

  
  int rank,size;
  MPI_Comm_size(comm,&size);
  
  MPI_Comm_rank(comm,&rank);
  if(rank==1)metatree->Fill();
  else {return;}
}
	     
//here we will show some examples on how to do some custom meta data operations...
//Case 2 is opening up the Tree and summing info one at a time....    
