import os,sys,shutil
import time,subprocess

def CreateWrapper(tot_nodes,ranks_per_node):
    #if (tot_ranks%tot_nodes)!=0:
     #   print "check your ranks and nodes numbers"
     #   sys.exit(1)
    #lines_interested["#COBALT -n", "aprun"]
    output_wrapper="run_wrapper_"+str(tot_nodes)+"_"+str(ranks_per_node)+".sh"
    with open("run_template_default.sh",'rt') as win:
        with open(output_wrapper,"wt") as wout:
            for line in win:
                #print line
                if "#COBALT -n" in line:
                    wout.write(line.replace("#COBALT -n", "#COBALT -n "+str(tot_nodes)))
                elif "aprun" in line:
                    wout.write(line.replace("aprun","aprun -n "+str(ranks_per_node*tot_nodes)+" -N "+str(ranks_per_node)))
                elif "$elapsedseconds" in line:
                    wout.write(line.replace("$elapsedseconds","$elapsedseconds "+str(tot_nodes)+" "+str(ranks_per_node)))
                else:
                    wout.write(line)
    os.chmod(output_wrapper,0775) #make it executable
    return output_wrapper
    
def RunExecutableInTheta(tot_nodes,ranks_per_node):
    output=CreateWrapper(tot_nodes,ranks_per_node)
    proc=-999
    proc = subprocess.check_output(["qsub",output])
    return proc


def WriteOutput(tot_nodes,ranks_per_nodes,process,infile):
    infile.write("tot_nodes "+str(tot_nodes)+" tot_ranks "+str(ranks_per_nodes)+ " Proc "+str(process))

filein = "theta_output_"+str(time.time())+".txt"
infile=open(filein,'w')
tot_nodes=[256]
rank_per_nodes=[1,2,4,8,16,32,64]
#tot_nodes=[64]
#rank_per_nodes=[1]
for i in range(0,len(tot_nodes)):
    for j in range(0,len(rank_per_nodes)):
        proc = RunExecutableInTheta(tot_nodes[i],rank_per_nodes[j])
        print proc
        if proc:
                 WriteOutput(tot_nodes[i],rank_per_nodes[j],proc,infile)
