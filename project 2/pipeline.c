/**
 * File: pipeline.c
 * ----------------
 * Presents the implementation of the pipeline routine.
 */

#include "pipeline.h"
#include <stdio.h>
#include <stdbool.h>

void pipeline(char *argv1[], char *argv2[], pid_t pids[]) {
  int fds[2];
  pipe(fds);
  pid_t read_child = fork();
  // Error checking
  if (read_child == -1) {
    printf("Error in creating read_child process.\n");
    return;
  }
  if (!read_child) {
    close(fds[1]);
    dup2(fds[0], STDIN_FILENO);
    close(fds[0]);
    execvp(argv2[0], argv2);
  }
  pids[1] = read_child;
  close(fds[0]);

  // Error checking
  pid_t write_child = fork();
  if (write_child == -1) {
    printf("Error in creating write_child process.\n");
    return;
  }

  if (!write_child) {
    close(fds[0]);
    dup2(fds[1], STDOUT_FILENO);
    close(fds[1]);
    execvp(argv1[0], argv1);
  }
  close(fds[1]);
  pids[0] = write_child;
}
