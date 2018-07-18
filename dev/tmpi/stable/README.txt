START OF FILE
########################################################
# WORKING TOWARDS THE STABILITY OF TMPIFILE AND FEW TO DO LIST FROM  
# JULY 10 MEETING.....
#
#SHORT NOTE IN DK2NU
#Dk2nu ntuples are basically created for the neutrino experiments
#that use FNAL-numi beam. It is supposed to contain information (kinematics 
#as well as not kinematics informations) of the neutrino starting from primary 
#proton to particle decaying to give neutrino.
#
#########################################################
contents:
src/:
  TMPIFile.cxx
  TClientInfo.cxx
  dk2nu.cc
  dkmeta.cc

include/:
   TMPIFile.h
   TClientInfo.h
   Linkdef.h
   dk2nu.h
   dkmeta.h
   dk_Linkdef.h
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
