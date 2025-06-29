#ifndef JINSOCK_H
#define JINSOCK_H

#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef __NR_pidfd_getfd
#define __NR_pidfd_getfd 438
#endif

static inline int pidfd_getfd(int pidfd, int targetfd, unsigned int flags) {
    return syscall(__NR_pidfd_getfd, pidfd, targetfd, flags);
}

#ifndef __NR_pidfd_open
#define __NR_pidfd_open 434
#endif

static inline int pidfd_open(pid_t pid, unsigned int flags) {
    return syscall(__NR_pidfd_open, pid, flags);
}
#define MAX_ENTRIES 1024

typedef struct {
    pid_t pid;
    int fd;
    char proc_name[256];
    char local_addr[64];
    char rem_addr[64];
    int rem_port;
} SocketEntry;

extern SocketEntry entries[MAX_ENTRIES];
extern int entry_count;
extern int recv_timeout_sec;
void trim_newline(char *s);
void print_usage();
void cmd_help();
int parse_ip_port(const char *hexipport, char *ipbuf, size_t ipbuflen, int *port);
int parse_ipv6_port(const char *hexipport, char *ipbuf, size_t ipbuflen, int *port);
int get_socket_inode_from_fd(pid_t pid, int fd, unsigned long long *inode);
int load_proc_name(pid_t pid, char *buf, size_t buflen);
int get_remote_addr_from_inode(pid_t pid, unsigned long long inode, char *ipbuf, size_t ipbuflen, int *port);
void cmd_search(const char *pattern);

int dup_socket_and_send(int pid, int fd, const char *data, size_t datalen);
int dup_socket_and_sendfile(int pid, int fd, const char *filepath);
int dup_socket_and_recv(int pid, int fd, const char *outfile);

#endif
