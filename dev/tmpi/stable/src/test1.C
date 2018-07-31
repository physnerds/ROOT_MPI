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
//TTree *tree = new TTree("tree","tree");
// tree->SetAutoFlush(400000000);
// Float_t px,py;
// tree->Branch("px",&px);
// tree->Branch("py",&py);
// gRandom->SetSeed(seed);
 //now we need to divide the collector and worker load from here..
 if(newfile->IsCollector())newfile->RunCollector();
 else{
TTree *tree = new TTree("tree","tree");
 tree->SetAutoFlush(400000000);
 Float_t px,py;
 tree->Branch("px",&px);
 tree->Branch("py",&py);
 gRandom->SetSeed(seed);
   for(int i=0;i<15;i++){
     //    std::cout<<"Event "<<i<<" local rank "<<newfile->GetMPILocalRank()<<std::endl;
     gRandom->Rannor(px,py);
     int sleep = int(gRandom->Gaus(3,1));
     if(i%5==0){
       int sleep = int(gRandom->Gaus(3,1));
        std::this_thread::sleep_for(std::chrono::seconds(sleep));
       
     }
      tree->Fill();
     //at the end of the event loop...put the sync function
       if(i%(newfile->GetSyncRate())==0){
	    newfile->Sync(); //this one as a worker...
	    tree->Reset();
	      }
   //do the syncing one more time
	      if(i%(newfile->GetSyncRate())!=0){
	     if(i==14){
	     newfile->Sync(); //to make sure that the final bit also gets written.   
	     // tree->Reset();
	      }
	     }
   //  printf("Get the size of the file %d\n",newfile->GetSize());
}
 
 //now the clean up part..
 //printf("Clean up part %d\n",newfile->GetMPILocalRank());
   // MPI_Finalize();
 
 //here we send empty buffers...put a barrier and so on....
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
   printf("End of test function execution rank %d\n",rank);  
  return 0;
}
#endif
