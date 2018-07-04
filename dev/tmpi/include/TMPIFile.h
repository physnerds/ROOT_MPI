//A TMemFile that utilizes MPI Libraries to create and Merge ROOT Files


#ifndef ROOT_TMPIFile
#define ROOT_TMPIFile

#include "TMemFile.h"
#include "TKey.h"
#include <vector>
#include <memory>

class TMPIFile : public TMemFile {
public:
  int argc;char** argv;
public:
//now we define the constructors, destructors and other needed files here...
//constructor similar to TMemFile but some extra arguments...
//later define what to do in constructor and destructor.....
  TMPIFile(const char *name,char *buffer, Long64_t size=0,Option_t *option="",const char *ftitle="",Int_t compress=4,Int_t np=2,Int_t split=0);//at least two processors and division of subgroups

  //another constructor where it takes TMemFile pointer as an argument...
virtual ~TMPIFile();
 ClassDef(TMPIFile,0)
};
#endif
