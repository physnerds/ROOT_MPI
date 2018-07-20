import os,sys,shutil
import subprocess
import time

def CheckForLogFile(proc,cwd,tot_nodes,tot_ranks,infile):
    logfile=proc+".output"
    errorfile=proc+".error"
    files=os.listdir(cwd)
    if errorfile in files:
        if os.path.getsize(errorfile)!=0:
            print "Job had error....Exiting"
            sys.exit(1)
    if logfile in files:
        with open(logfile,'r') as lfile:
            for line in lfile:
                #print line
                if "ElapsedTime" in line:
                    #print line
                    newline=line.replace("ElapsedTime= ","")
                    WriteOutput(tot_nodes,tot_ranks,newline,infile)
                    
    else:
        print "No logfiles now sleeping for 2  minutes"
        time.sleep(120)
        CheckForLogFile(proc,cwd,tot_nodes,tot_ranks,infile)
        
def CreateWrapper(tot_nodes,tot_ranks):
    if (tot_ranks%tot_nodes)!=0:
        print "check your ranks and nodes numbers"
        sys.exit(1)
    #lines_interested["#COBALT -n", "aprun"]
    output_wrapper="run_wrapper_"+str(tot_nodes)+"_"+str(tot_ranks)+".sh"
    with open("run_template.sh",'rt') as win:
        with open(output_wrapper,"wt") as wout:
            for line in win:
                #print line
                if "#COBALT -n" in line:
                    wout.write(line.replace("#COBALT -n", "#COBALT -n "+str(tot_nodes)))
                elif "aprun" in line:
                    wout.write(line.replace("aprun","aprun -n "+str(tot_ranks)+" -N "+str(tot_ranks/tot_nodes)))
                else:
                    wout.write(line)
    os.chmod(output_wrapper,0775) #make it executable
    return output_wrapper
    
def RunExecutableInTheta(tot_nodes,tot_ranks):
    output=CreateWrapper(tot_nodes,tot_ranks)
    proc=subprocess.check_output(["qsub",output])
    proc=proc[:-1] #remove the trailing end of the line output
    return proc

def WriteOutput(tot_nodes,tot_ranks,est_time,infile):
    infile.write("tot_nodes="+str(tot_nodes)+" tot_ranks="+str(tot_ranks)+" Est_time="+str(est_time)+"\n")
    


filein=("theta_output_"+str(time.time())+".txt")
proc=RunExecutableInTheta(8,8)
infile=open(filein,'w')
print proc
proc=proc[:-1] #remove the trailing end of line output
print len(proc)
if proc:
    #print proc,output
    cwd=os.getcwd()
    #CreateWrapper(8,16)
    CheckForLogFile(proc,cwd,4,8,infile)

