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
#include <iostream>

void test1(){
TMPIFile *newfile = new TMPIFile("Simple_MPIFile.root","RECREATE");
int seed = newfile->GetMPILocalSize()+newfile->GetMPIColor()+newfile->GetMPILocalRank();
 int sync_rate = newfile->GetSyncRate();
 //now we need to divide the collector and worker load from here..
 if(newfile->IsCollector())newfile->RunCollector(); //this one keeps the collector going...
 else{
TTree *tree = new TTree("tree","tree");
 tree->SetAutoFlush(400000000);
 Float_t px,py;
 tree->Branch("px",&px);
 tree->Branch("py",&py);
 gRandom->SetSeed(seed);
 //total number of entries
 Int_t tot_entries = 15;
   for(int i=0;i<tot_entries;i++){
     //    std::cout<<"Event "<<i<<" local rank "<<newfile->GetMPILocalRank()<<std::endl;
     gRandom->Rannor(px,py);
     int sleep = int(gRandom->Gaus(3,1));
     //simulating the reco time...per 5 events here....
     if(i%5==0){
       int sleep = int(gRandom->Gaus(3,1));
        std::this_thread::sleep_for(std::chrono::seconds(sleep));
       
     }
      tree->Fill();
      //at the end of the event loop...put the sync function
      //************START OF SYNCING IMPLEMENTATION FROM USERS' SIDE**********************
       if(i%(newfile->GetSyncRate())==0){
	    newfile->Sync(); //this one as a worker...
	    tree->Reset();
	      }
   //do the syncing one more time
	      if(i%(newfile->GetSyncRate())!=0){
	     if(i==tot_entries-1){
	     newfile->Sync(); //to make sure that the final bit also gets written.   
	      }
	     }
	      //************END OF SYNCING IMPLEMENTATION FROM USERS' SIDE***********************
}
 
 }
 newfile->MPIClose();
 // newfile->Close();
}
#ifndef __CINT__
int main(int argc,char* argv[]){
  int rank,size;
  MPI_Init(&argc,&argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  //  printf("start of rank %d\n",rank);
  test1();
  MPI_Finalize();
  //  printf("End of test function execution rank %d\n",rank);  
  return 0;
}
#endif
