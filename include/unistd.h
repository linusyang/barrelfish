/*
 * Copyright (c) 2007, 2008, 2009, 2011, 2012, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef __BF_UNISTD_H
#define __BF_UNISTD_H

#include <stddef.h>
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h> // for pid_t
#include <sys/socket.h>

#define	R_OK 4
#define	W_OK 2
#define	X_OK 1
#define	F_OK 0

#define	STDIN_FILENO  0
#define	STDOUT_FILENO 1
#define	STDERR_FILENO 2

struct stat;
extern char **environ;

void _exit(int status);
int read(int fd, void *buf, size_t len);
int write(int fd, const void *buf, size_t len);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int ftruncate(int fd, int length);
int access(const char*pathname,int mode);
int chdir(const char*pathname);
int mkdir(const char *pathname, int mode);
int rmdir(const char*pathname);
int link(const char *oldpath, const char *newpath);
int symlink(const char *oldpath, const char *newpath);
int unlink(const char*pathname);
int dup(int oldfd);
int isatty(int fd);
int dup2(int oldfd, int newfd);
int pipe(int pipefd[2]);
char *getcwd(char *buf, size_t size);
pid_t getpid(void);
pid_t getppid(void);
void *sbrk(intptr_t increment);
long gethostid(void);
uid_t geteuid(void);
uid_t getuid(void);
int fsync(int fd);
pid_t fork(void);
int execv(const char *path, char *const argv[]);
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
           struct timeval *timeout);
int chown(const char *path, uid_t owner, gid_t group);
int chmod(const char *path, mode_t mode);
int getgroups(int size, gid_t grouplist[]);
int setgroups(int size, gid_t grouplist[]);
int initgroups(const char *username, gid_t group);
unsigned int alarm(unsigned int seconds);
pid_t setsid(void);


#endif // __BF_UNISTD_H
