START OF FILE
########################################################
# WORKING TOWARDS THE STABILITY OF TMPIFILE AND FEW TO DO LIST FROM  
# JULY 10 MEETING.....
#
#
#
#########################################################
contents:
src/:
  TMPIFile.cxx
  TClientInfo.cxx

include/:
   TMPIFile.h
   TClientInfo.h
   Linkdef.h
   
./
  Makefile
  setenv.sh
##########################################################

Requirements:
mpich: mpich.org (latest version)
ROOT: (preferably greater than version 6)

#######################################################
Notes on MakeFile
Change the include path to your mpich-build include path ($MPINCLUDEPATH)
########################################################
Notes on setenv.sh
$(MPINCLUDES) = path to mpi headers
$(MPLIBS) = path to mpi libraries..

#########################################################
END OF FILE
