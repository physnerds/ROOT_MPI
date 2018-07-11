#ifndef ROOT_TClientInfo
#define ROOT_TClientInfo

#include "TMemFile.h"
#include "TTimeStamp.h"
#include "TFile.h"
class TClientInfo{

 public:
  TFile *fFile;
  TString fLocalName;
  UInt_t fContactsCount;
  TTimeStamp fLastContact;
  Double_t fTimeSincePrevContact;

  TClientInfo();//default constructor
  TClientInfo(const char *filename, UInt_t clientID);//another constructor
  virtual ~TClientInfo();

  void Set(TFile *file);
  void R__MigrateKey(TDirectory *destination, TDirectory *source);
  void R__DeleteObject(TDirectory *dir, Bool_t withReset);

ClassDef(TClientInfo,0);
};
#endif
