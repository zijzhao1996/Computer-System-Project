/**
 * File: subprocess.cc
 * -------------------
 * Presents the implementation of the subprocess routine.
 */

#include "subprocess.h"
using namespace std;

subprocess_t subprocess(char *argv[], bool supplyChildInput, bool ingestChildOutput) throw (SubprocessException) {
	
	int supplyChildInput_fds[2];
	int ingestChildOutput_fds[2];
	int supplyfd = kNotInUse;
	int ingestfd = kNotInUse;

	if (supplyChildInput) {
		pipe(supplyChildInput_fds);
  		supplyfd = supplyChildInput_fds[1];
	}
	if (ingestChildOutput) {
		pipe(ingestChildOutput_fds);
    	ingestfd = ingestChildOutput_fds[0];
  	}

  	pid_t pid = fork();
  	// Error checking
  	if (pid < 0){
    	throw SubprocessException("Error in forking child process.");
  	}

  	subprocess_t sp =  {pid, supplyfd, ingestfd};
  	
  	// In child process
  	if (sp.pid == 0){
    	if (supplyChildInput){
      		close(supplyChildInput_fds[1]);
      		// Using dup2 and close to redirect the read end to stdin
      		dup2(supplyChildInput_fds[0], 0);
      		close(supplyChildInput_fds[0]);
    	}
    	if (ingestChildOutput){
      		close(ingestChildOutput_fds[0]);
      		// Using dup2 and close to redirect the write end to stdout
      		dup2(ingestChildOutput_fds[1], 1);
      		close(ingestChildOutput_fds[1]);
    	}
    	execvp(argv[0], argv);
    	// Error checking
    	throw SubprocessException("Error in executing execp.");
  	}

  	if (supplyChildInput){
    	close(supplyChildInput_fds[0]);
  	}
  	if (ingestChildOutput){
    	close(ingestChildOutput_fds[1]);
  	}
  	return sp;
}

	
