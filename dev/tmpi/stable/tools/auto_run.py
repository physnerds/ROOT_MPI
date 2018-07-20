import os,sys,shutil
import subprocess
import time

def CheckForLogFile(logfile,cwd):
    files=os.listdir(cwd)
    if logfile in files:
        print "DO STH"
    else:
        print "No logfiles now sleeping for 5  seconds"
        time.sleep(5 )
        CheckForLogFile(logfile,cwd)
        
def CreateWrapper(tot_nodes,tot_ranks):
    if (tot_ranks%tot_nodes)!=0:
        print "check your ranks and nodes numbers"
        sys.exit(1)
    #lines_interested["#COBALT -n", "aprun"]
    output_wrapper="run_wrapper_"+str(tot_nodes)+"_"+str(tot_ranks)+".sh"
    with open("run_template.sh",'rt') as win:
        with open(output_wrapper,"wt") as wout:
            for line in win:
                print line
                if "#COBALT -n" in line:
                    wout.write(line.replace("#COBALT -n", "#COBALT -n "+str(tot_nodes)))
                elif "aprun" in line:
                    wout.write(line.replace("aprun","aprun -n "+str(tot_ranks)+" -N "+str(tot_ranks/tot_nodes)))
                else:
                    wout.write(line)
            
    
#subprocess.check_output(['source try_time.sh'])
#subprocess.call(['./try_time.sh'])
a=str(subprocess.check_output(['./try_time.sh']))
if a:
    print a
    cwd=os.getcwd()
    CreateWrapper(8,16)
    logfile=os.getenv("LOGFILE")
    #CheckForLogFile("2.output",cwd)


