#include "TMPIFile.h"
#include "TFile.h"
#include "TROOT.h"
#include "TRandom.h"
#include "TTree.h"
#include "TSystem.h"
#include "TMemFile.h"
#include "TH1D.h"
#include "dk2nu.h"
#include "dkmeta.h"
#include <chrono>
#include <thread>
#include "mpi.h"

void test(){
  //first get a root file and copy into buffer.
  // TFile *_file = new TFile("tempfile_coming.root","READONLY");
  void R__MigrateKey(TDirectory *source, TDirectory *destination);
  void FillDkMetaTree(TTree *t,TMPIFile *infile,bsim::DkMeta* dkmeta,int sync_rate);
  void FillDk2NuTree(TTree *t,TMPIFile *infile,bsim::Dk2Nu* dk2nu,int Seed,int sync_rate);
  TMPIFile *newfile = new TMPIFile("Trial_MPIFile.root","RECREATE");
  int seed = newfile->GetMPILocalSize()+newfile->GetMPIColor()+newfile->GetMPILocalRank();
  int sync_rate = newfile->GetSyncRate();
  bsim::Dk2Nu* dk2nu = new bsim::Dk2Nu;
  bsim::DkMeta* dkmeta = new bsim::DkMeta;
  //create the trees.....
  TTree *fDk2nu = new TTree("dk2nuTree","testing with dk2nu struc");
  fDk2nu->Branch("dk2nu","bsim::Dk2Nu",&dk2nu,32000,99);
  //now the dkmeta tree
  TTree *fDkMeta = new TTree("dkmetaTree","testing with meta struc");
  fDkMeta->Branch("dkmeta","bsim::DkMeta",&dkmeta,32000,99);
  // if(!newfile->IsCollector()){ //this one added to make sure that collector only collects and does nothing....
  //    FillDkMetaTree(fDkMeta,newfile,dkmeta,sync_rate);
  FillDk2NuTree(fDk2nu,newfile,dk2nu,seed,sync_rate);
    printf("Filled by rank %d\n",newfile->GetMPILocalRank());
    // newfile->MPIWrite();
    // MPI_Finalize();
  // fDkMeta->Fill();
  // fDk2nu->Fill();
   
    //  }
    // newfile->Write();

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
void FillDkMetaTree(TTree *fDkMeta,TMPIFile *infile,bsim::DkMeta* dkmeta,int sync_rate){
  dkmeta->pots = 1.0E5;
  dkmeta->beamsim = "mpi_run_test";
  dkmeta->physics = "neutrino decay info mock";
  dkmeta->tgtcfg = "no_tgt";
  dkmeta->physcuts = "sth_sth";
  
  
  dkmeta->location.clear();
  bsim::Location alocation(0.0,0.0,57400.00,"DUNE_NEAR_DET");
  dkmeta->location.push_back(alocation);
  fDkMeta->Fill();
    infile->MPIWrite();
}
//now fill the dk2nu information....
void FillDk2NuTree(TTree *fDk2Nu,TMPIFile *infile,bsim::Dk2Nu* dk2nu,int seed,int sync_rate){
  //here we fill the so called events....
  //clear some stuffs here
  TRandom *rand = new TRandom(seed);
  Float_t px,py,pz,E;
  dk2nu->job = seed;
  for(int i=0;i<10;i++){
    if(!infile->IsCollector()){
    int sleep = int(rand->Gaus(3,1));
     if(i%5==0){
 
    	std::this_thread::sleep_for(std::chrono::seconds(sleep));
      // printf("now sleeping \n");
     }
    gRandom->Rannor(px,py);
    pz = px*23.3442+py*1.213;
    E = sqrt(px*px+py*py+pz*pz);
    bsim::NuRay Nuinfo(px,py,pz,E,1.0);
    dk2nu->nuray.push_back(Nuinfo);
    for(int j=0;j<(sleep+2);j++){
      bsim::Ancestor a;
      TRandom *erand = new TRandom(seed+1243);
      Float_t startx,starty,startz;
      gRandom->Rannor(startx,starty);
      a.startx = startx;
      a.starty = starty;
      a.startz = erand->Gaus(startx);
      a.startpx = erand->Gaus(a.startz);
      a.startpy = erand->Gaus(a.startpx);
      a.startpz = erand->Gaus(a.startpy);
      dk2nu->ancestor.push_back(a);
      delete erand;
    }
    }//<---we want to make sure that only workers do the computational/evt reco. job---->
    //  fDk2Nu->Fill();
    if((i%sync_rate==0)||(i==9)){
      fDk2Nu->Fill();
      infile->MPIWrite(i,10);
      dk2nu->nuray.clear();
      dk2nu->ancestor.clear();
      dk2nu->vint.clear();
    }

    
  }
  delete rand;
}

#endif
