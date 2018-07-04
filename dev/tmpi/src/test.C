#include "TMPIFile.h"
#include "TFile.h"
#include "TROOT.h"
#include "TSystem.h"
void test(){
  //first get a root file and copy into buffer.
  TFile *_file = new TFile("tempfile_coming.root","READONLY");
  Long64_t _count = _file->GetSize();
  char *_buf = new char[_count];
  _file->WriteBuffer(_buf,_count);
  TMPIFile *somefile = new TMPIFile("name.root",_buf,_count);
}
#ifndef ___CINT__
int main(int argc,char* argv[]){
  gSystem->Load("lib/libTMPI.so");
  test();
  return 0;
}
#endif
