#include "TMPIFile.h"
#include "TFile.h"
#include "TROOT.h"
#include "TRandom.h"
#include "TTree.h"
#include "TSystem.h"
#include "TMemFile.h"
void test(){
  //first get a root file and copy into buffer.
  // TFile *_file = new TFile("tempfile_coming.root","READONLY");
 void R__MigrateKey(TDirectory *source, TDirectory *destination);

 TMPIFile *newfile = new TMPIFile("Trial_MPIFile.root","RECREATE",5);
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
  newfile->MPIWrite();
  // newfile->PurgeEveryThing();
  //newfile->Close();

}
#ifndef ___CINT__
int main(int argc,char* argv[]){
  test();
  MPI_Finalize();
  return 0;
}

void R__DeleteObject(TDirectory *dir, Bool_t withReset)
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
 void R__MigrateKey(TDirectory *destination, TDirectory *source)
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

#endif
