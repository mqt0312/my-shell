# my-shell

A simple implementation of the Linux shell.
 - Receive command with multiple parameters
 - Fork a child process
 - Either wait for that child process to be done, or if '&' was specified, resume and execute next command immediately (effectively in parallel)
 - Catch simple signals like SIGINT, SIGCHILD
