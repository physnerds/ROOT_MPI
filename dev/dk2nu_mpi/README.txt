####################################################
# split_mpiparallelmerge where the mock dk2nu ntuples are generated
#and merged. Dk2nu ntuples are basically created for the neutrino experiments
#that use FNAL-numi beam. It is supposed to contain information (kinematics 
#as well as not kinematics informations) of the neutrino starting from primary 
#proton to particle decaying to give neutrino.
############################################################3
#	INSTRUCTIONS
#Makefile: Make sure the environment variables are pointing in right paths
#Makefile generates the libdk.so file. Make sure the libdk.so file is in
#$LD_LIBRARY_PATH or alternately copy it to the $ROOTSYS/lib if possible.
#
#Generates split_mpiparallelMerger 
#
##############################################################
