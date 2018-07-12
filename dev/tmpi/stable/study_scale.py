import os,sys,subprocess
import time
import ROOT
import matplotlib.pyplot as plt
#Study as a function of number of processors
time_list=[]
#subprocess.call("run_process.sh") 
for i in range(1,11):
    np = i*10
    print "processing ",np," process run"
    command = "mpirun -np "+str(np)+" ./bin/test"
    then = time.time()
    os.system(command)
    now = time.time()
    diff= now-then
    time_list.append(diff)

ranks=[]
for i in range(len(time_list)):
    print (i+1)*10,time_list[i]
    ranks.append((i+1)*10)

plt.plot(ranks,time_list,'ro')
plt.show()
