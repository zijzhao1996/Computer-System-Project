/**
 * File: stsh.cc
 * -------------
 * Defines the entry point of the stsh executable.
 */

#include "stsh-parser/stsh-parse.h"
#include "stsh-parser/stsh-readline.h"
#include "stsh-parser/stsh-parse-exception.h"
#include "stsh-signal.h"
#include "stsh-job-list.h"
#include "stsh-job.h"
#include "stsh-parse-utils.h"
#include "stsh-process.h"
#include <cstring>
#include <iostream>
#include <string>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>  // for fork
#include <signal.h>  // for kill
#include <sys/wait.h>
#include <cassert>
using namespace std;

static STSHJobList joblist; 
// Usage information.
static const string kFgUsage = "Usage: fg <jobid>.";
static const string kBgUsage = "Usage: bg <jobid>.";
static const string kSlayUsage = "Usage: slay <jobid> <index> | <pid>.";
static const string kHaltUsage = "Usage: halt <jobid> <index> | <pid>.";
static const string kContUsage = "Usage: cont <jobid> <index> | <pid>.";
// Function declaration
static void fg(const command& cmd);
static void bg(const command& cmd);
static void slay(const command& cmd);
static void halt(const command& cmd);
static void cont(const command& cmd);
static void update_Joblist(pid_t pid, STSHProcessState state);

/**
 * Function: handleBuiltin
 * -----------------------
 * Examines the leading command of the provided pipeline to see if
 * it's a shell builtin, and if so, handles and executes it.  handleBuiltin
 * returns true if the command is a builtin, and false otherwise.
 */
static const string kSupportedBuiltins[] = {"quit", "exit", "fg", "bg", "slay", "halt", "cont", "jobs"};
static const size_t kNumSupportedBuiltins = sizeof(kSupportedBuiltins)/sizeof(kSupportedBuiltins[0]);
static bool handleBuiltin(const pipeline& pipeline) {
  const command& cmd = pipeline.commands[0];
  const string& command = cmd.command;
  auto iter = find(kSupportedBuiltins, kSupportedBuiltins + kNumSupportedBuiltins, command);
  if (iter == kSupportedBuiltins + kNumSupportedBuiltins) return false;
  size_t index = iter - kSupportedBuiltins;

  switch (index) {
  case 0:
  case 1: exit(0);
  case 2: fg(cmd); break;
  case 3: bg(cmd); break;
  case 4: slay(cmd); break;
  case 5: halt(cmd); break;
  case 6: cont(cmd); break;
  case 7: cout << joblist; break;
  default: throw STSHException("Internal Error: Builtin command not supported."); // or not implemented yet
  }

  return true;
}

/**
 * Function: handleSIGCHLD
 * -------------------
 * Signal handler for SIGCHLD.
 */
static void handleSIGCHLD(int sig) {
  pid_t pid;
  while (true) {
    int status;
    pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);
    if (pid <= 0) break;

    STSHProcessState state = kTerminated;
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      state = kTerminated;
    } else if (WIFSTOPPED(status)) {
      state = kStopped;
    } else if (WIFCONTINUED(status)) {
      state = kRunning;
    }
    update_Joblist(pid, state);
  }
  if (!joblist.hasForegroundJob()) {
    tcsetpgrp(STDIN_FILENO, getpgrp());
  }
}

/**
 * Function: signal_pass
 * -------------------
 * Passes the signal to the foreground job.
 */
static void signal_pass(int sig) {
  if (joblist.hasForegroundJob()) {
    kill(-joblist.getForegroundJob().getGroupID(), sig);
  }
}

/**
 * Function: installSignalHandlers
 * -------------------------------
 * Installs user-defined signals handlers for four signals.
 */
static void installSignalHandlers() {
  installSignalHandler(SIGQUIT, [](int sig) { exit(0); });
  installSignalHandler(SIGTTIN, SIG_IGN);
  installSignalHandler(SIGTTOU, SIG_IGN);
  installSignalHandler(SIGCHLD, handleSIGCHLD);
  installSignalHandler(SIGINT, signal_pass);
  installSignalHandler(SIGTSTP, signal_pass);
}


/**
 * Function: getArglen
 * -------------------
 * Gets cmd.tokens valid length.
 */
static size_t getArglen(const command& cmd) {
  for (size_t i = 0; i < kMaxArguments + 1; i++) {
    if (cmd.tokens[i] == NULL) 
      return i;
  }
  return kMaxArguments;
}

/**
 * Function: update_Joblist
 * -------------------
 * Updates the joblist.
 */
static void update_Joblist(pid_t pid, STSHProcessState state) {
  if (!joblist.containsProcess(pid)) return;
  STSHJob& job = joblist.getJobWithProcess(pid);
  assert(job.containsProcess(pid));
  STSHProcess& process = job.getProcess(pid);
  process.setState(state);
  joblist.synchronize(job);
}

/**
 * Function: waitForFgJobToFinish
 * -------------------
 * Makes main process hang for foreground job to finish.
 */
static void waitForFgJobToFinish() {
  sigset_t additions, existingmask;
  sigemptyset(&additions);
  sigaddset(&additions, SIGCHLD);
  sigprocmask(SIG_BLOCK, &additions, &existingmask);

  while (joblist.hasForegroundJob()) {
    // atomic version of pause
    sigsuspend(&existingmask);
  }
  sigprocmask(SIG_UNBLOCK, &additions, NULL);
}

/**
 * Function: getProcess
 * -------------------
 * Gets the process from input.
 */
static STSHProcess& getProcess(const command& cmd, const string& usage) {
  size_t argc = getArglen(cmd);
  if (argc < 1 || argc > 2) throw STSHException(usage);
  size_t pid;
  if (argc == 1) {
    pid = parseNumber(cmd.tokens[0], usage);
    if (!joblist.containsProcess(pid)) {
      throw STSHException("That pid doesn't belong to any valid process.");
    }
  } else {
    size_t jobnum = parseNumber(cmd.tokens[0], usage);
    size_t processnum = parseNumber(cmd.tokens[1], usage);
    if (!joblist.containsJob(jobnum)) {
      throw STSHException("Job number is invalid.");
    }
    vector<STSHProcess>& processes = joblist.getJob(jobnum).getProcesses();
    if (processnum >= processes.size()) {
      throw STSHException("Job doesn't have such index.");
    }
    pid = processes[processnum].getID();
  }
  return joblist.getJobWithProcess(pid).getProcess(pid);
}

/**
 * Function: getBgjob
 * -------------------
 * Gets the background job from input.
 */
static STSHJob& getBgjob(const command& cmd, const string& usage, const string& caller) {
  size_t argc = getArglen(cmd);
  if (argc != 1) throw STSHException(usage);
  size_t jobnum = parseNumber(cmd.tokens[0], usage);
  if (!joblist.containsJob(jobnum)) {
    throw STSHException(caller + " " + to_string(jobnum) + ": No such job.");
  }
  STSHJob& job = joblist.getJob(jobnum);
  assert(job.getState() != kForeground);
  return job;
}

/**
 * Function: fg
 * -------------------
 * Implementation for fg.
 */
static void fg(const command& cmd) {
  const string& usage = kFgUsage;
  STSHJob& job = getBgjob(cmd, usage, "fg");
  for (const auto& p : job.getProcesses()) {
    if (p.getState() == kStopped) {
      kill(-job.getGroupID(), SIGCONT);
      break;
    }
  }
  job.setState(kForeground);
  waitForFgJobToFinish();
}

/**
 * Function: bg
 * -------------------
 * Implementation for bg.
 */
static void bg(const command& cmd) {
  const string& usage = kBgUsage;
  STSHJob& job = getBgjob(cmd, usage, "bg");
  for (const auto& p : job.getProcesses()) {
    if (p.getState() == kStopped) {
      kill(-job.getGroupID(), SIGCONT);
      break;
    }
  }
}

/**
 * Function: slay
 * -------------------
 * Implementation for slay.
 */
static void slay(const command& cmd) {
  const string& usage = kSlayUsage;
  STSHProcess& process = getProcess(cmd, usage);
  kill(process.getID(), SIGKILL);
}

/**
 * Function: halt
 * -------------------
 * Implementation for halt.
 */
static void halt(const command& cmd) {
  const string& usage = kHaltUsage;
  STSHProcess& process = getProcess(cmd, usage);
  if (process.getState() == kRunning) {
    kill(process.getID(), SIGTSTP);
  }
}

/**
 * Function: cont
 * -------------------
 * Implementation for cont.
 */
static void cont(const command& cmd) {
  const string& usage = kContUsage;
  STSHProcess& process = getProcess(cmd, usage);
  if (process.getState() == kStopped) {
    kill(process.getID(), SIGCONT);
  }
}


/**
 * Function: createProcess
 * -------------------
 * Creates a new process on behalf of the provided pipeline and
 * command id, add the process to the given job.
 */
static void createProcess(STSHJob& job, const pipeline& p, int cmdid, int fds[]) {
  const command& cmd = p.commands[cmdid];
  int numCommands = p.commands.size();
  bool first = cmdid == 0;
  bool last = cmdid == (numCommands - 1);
  pid_t pid = fork();

  if (pid == 0) {
    setpgid(0, job.getGroupID());
    // close unrelated fds
    for (int i = 0; i < (numCommands - 1) * 2; i++) {
      if (!(i >= (cmdid-1)*2 && i < (cmdid+1)*2)) close(fds[i]);
    }
    // close related fds
    if (!first) {
      close(fds[cmdid * 2 - 1]); 
      // redirect stdin to the previous fd_read
      dup2(fds[(cmdid-1) * 2], 0); 
      close(fds[(cmdid-1) * 2]);
    } else if (!p.input.empty()) {
      int infd = open(p.input.c_str(), O_RDONLY);
      // redirect stdin to infd
      dup2(infd, 0); 
      close(infd);
    }
    if (!last) {
      close(fds[cmdid * 2]); 
      // redirect stdout to the next fd_write
      dup2(fds[cmdid * 2 + 1], 1); 
      close(fds[cmdid * 2 + 1]);
    } else if (!p.output.empty()) {
      int outfd = open(p.output.c_str(), O_WRONLY | O_TRUNC | O_CREAT, 0644);
      // redirect stdout to outfd
      dup2(outfd, 1); 
      close(outfd);
    }

    char *new_argv[kMaxArguments + 2];
    new_argv[0] = (char *) cmd.command;
    for (size_t i = 1; i <= kMaxArguments + 1; i++) {
      new_argv[i] = cmd.tokens[i - 1];
      if (new_argv[i] == NULL) break;
    }
    execvp(cmd.command, new_argv);
    throw STSHException(string(cmd.command) + ": Command not found.");
  }
  if (first) setpgid(pid, pid);
  job.addProcess(STSHProcess(pid, cmd));
}

/**
 * Function: createJob
 * -------------------
 * Creates a new job on behalf of the provided pipeline.
 */
static void createJob(const pipeline& p) {
  STSHJobState jobState = kForeground;
  if (p.background) jobState = kBackground;
  STSHJob& job = joblist.addJob(jobState);
  const vector<command>& commands = p.commands;
  size_t numCommands = commands.size();
  int fds[(numCommands - 1) * 2];
  for (size_t i = 0; i < numCommands - 1; i++) {
    pipe(fds + i * 2);
  }
  for (size_t i = 0; i < numCommands; i++) {
    createProcess(job, p, i, fds);
  }
  for (size_t i = 0; i < (numCommands - 1) * 2; i++) {
    close(fds[i]);
  }
  if (p.background) {
    cout << "[" << job.getNum() << "]";
    for (const auto& p : job.getProcesses()) {
      cout << " " << p.getID();
    }
    cout << endl;
    return;
  }
  // give stdin control to foreground process group
  tcsetpgrp(STDIN_FILENO, job.getGroupID());
  waitForFgJobToFinish();
}

/**
 * Function: main
 * --------------
 * Defines the entry point for a process running stsh.
 * The main function is little more than a read-eval-print
 * loop (i.e. a repl).
 */
int main(int argc, char *argv[]) {
  pid_t stshpid = getpid();
  installSignalHandlers();
  rlinit(argc, argv); 
  while (true) {
    string line;
    if (!readline(line)) break;
    if (line.empty()) continue;
    try {
      pipeline p(line);
      bool builtin = handleBuiltin(p);
      if (!builtin) createJob(p);
    } catch (const STSHException& e) {
      cerr << e.what() << endl;
      if (getpid() != stshpid) exit(0); // if exception is thrown from child process, kill it
    }
  }

  return 0;
}
