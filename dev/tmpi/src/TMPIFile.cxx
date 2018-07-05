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
		   Int_t np,Int_t split):TMemFile(name,buffer,size,option,ftitle,compress){
  if(buffer)printf("buffer of non 0 size received\n");
  //Initialize MPI if it is not already initialized...
  int flag;
  MPI_Initialized(&flag);
  if(!flag) MPI_Init(&argc,&argv); 
}
TMPIFile::~TMPIFile(){
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

