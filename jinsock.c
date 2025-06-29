#include "jinsock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <linux/limits.h>
#include <linux/net.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#ifndef __NR_pidfd_open
#define __NR_pidfd_open 434
#endif

#ifndef __NR_pidfd_getfd
#define __NR_pidfd_getfd 438
#endif

SocketEntry entries[MAX_ENTRIES];
int entry_count = 0;


void trim_newline(char *s) {
    size_t len = strlen(s);
    if (len > 0 && s[len-1] == '\n') s[len-1] = 0;
}

void cmd_help() {
    printf(
        "Commands:\n"
        "  help                 - Show this help\n"
        "  search [pattern]     - List sockets, optionally filter by pid or process name\n"
        "  select <index>       - Select a socket from the search results\n"
        "  send <string>        - Send string to selected socket\n"
        "  sendf <file>         - Send file content to selected socket\n"
        "  rec [file]           - Receive from socket with timeout, output to stdout or file\n"
        "  timeout <seconds>    - Set receive timeout (default 5 sec)\n"
        "  quit                 - Exit\n"
    );
}

int parse_ip_port(const char *hexipport, char *ipbuf, size_t ipbuflen, int *port) {
    // hexipport format: "0100007F:1F90" (IP:port in hex)
    unsigned int p;
    if (strlen(hexipport) < 13) return -1;
    char ip_hex[9] = {0};
    strncpy(ip_hex, hexipport, 8);
    sscanf(hexipport, "%8X:%X", &p, &p);
    if (sscanf(hexipport, "%8X:%X", &p, &p) != 2) return -1;

    // Parse IP and port:
    unsigned int ipval, portval;
    if (sscanf(hexipport, "%8X:%X", &ipval, &portval) != 2) return -1;

    // ipval is in little endian hex, convert to IP string:
    unsigned char bytes[4];
    bytes[0] = (ipval >> 0) & 0xFF;
    bytes[1] = (ipval >> 8) & 0xFF;
    bytes[2] = (ipval >> 16) & 0xFF;
    bytes[3] = (ipval >> 24) & 0xFF;

    snprintf(ipbuf, ipbuflen, "%u.%u.%u.%u", bytes[0], bytes[1], bytes[2], bytes[3]);
    *port = portval;
    return 0;
}

int parse_ipv6_port(const char *hexipport, char *ipbuf, size_t ipbuflen, int *port) {
    // Format: 32 hex digits for IPv6 + :port (e.g. "00000000000000000000000000000001:1F90")
    if (strlen(hexipport) < 37) return -1;
    char iphex[33];
    strncpy(iphex, hexipport, 32);
    iphex[32] = 0;
    sscanf(hexipport + 33, "%X", port);

    unsigned char ip[16];
    for (int i=0; i<16; i++) {
        char bytehex[3] = {iphex[i*2], iphex[i*2+1], 0};
        ip[15-i] = (unsigned char)strtol(bytehex, NULL, 16);
    }
    inet_ntop(AF_INET6, ip, ipbuf, ipbuflen);
    return 0;
}

// Check if the socket inode matches one from /proc/pid/fd/<fd>
int get_socket_inode_from_fd(pid_t pid, int fd, unsigned long long *inode) {
    char fdlink[PATH_MAX];
    char linktarget[PATH_MAX];
    snprintf(fdlink, sizeof(fdlink), "/proc/%d/fd/%d", pid, fd);
    ssize_t r = readlink(fdlink, linktarget, sizeof(linktarget)-1);
    if (r < 0) return -1;
    linktarget[r] = 0;
    // Expected format: "socket:[1234567]"
    unsigned long long ino = 0;
    if (sscanf(linktarget, "socket:[%llu]", &ino) == 1) {
        *inode = ino;
        return 0;
    }
    return -1;
}

int load_proc_name(pid_t pid, char *buf, size_t buflen) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (!fgets(buf, buflen, f)) {
        fclose(f);
        return -1;
    }
    trim_newline(buf);
    fclose(f);
    return 0;
}

// Extract remote IP/port from /proc/<pid>/net/tcp or tcp6 by inode
int get_remote_addr_from_inode(pid_t pid, unsigned long long inode, char *ipbuf, size_t ipbuflen, int *port) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/net/tcp", pid);
    FILE *f = fopen(path, "r");
    if (!f) {
        // Try tcp6
        snprintf(path, sizeof(path), "/proc/%d/net/tcp6", pid);
        f = fopen(path, "r");
        if (!f) return -1;
    }
    char line[512];
    fgets(line, sizeof(line), f); // skip header
    while (fgets(line, sizeof(line), f)) {
        unsigned int sl;
        char local_addr[64], rem_addr[64], st[8], tx_queue[16], rx_queue[16], tr[8], tm_when[16], retrnsmt[16];
        unsigned long long local_inode;
        // Format from /proc/net/tcp:
        // sl  local_address rem_address st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode
        // We only need rem_address, inode
        if (sscanf(line,
            "%u: %63s %63s %7s %15s %15s %7s %15s %15s %*u %*u %llu",
            &sl, local_addr, rem_addr, st, tx_queue, rx_queue, tr, tm_when, retrnsmt, &local_inode) == 10) {
            if (local_inode == inode) {
                fclose(f);
                // Parse rem_address: port is hex after ':'
                if (strlen(rem_addr) == 13) {
                    // IPv4 case
                    return parse_ip_port(rem_addr, ipbuf, ipbuflen, port);
                } else if (strlen(rem_addr) == 37) {
                    return parse_ipv6_port(rem_addr, ipbuf, ipbuflen, port);
                }
                return -1;
            }
        }
    }
    fclose(f);
    return -1;
}

void cmd_search(const char *pattern) {
    entry_count = 0;
    DIR *proc = opendir("/proc");
    if (!proc) {
        perror("opendir /proc");
        return;
    }
    struct dirent *dent;
    while ((dent = readdir(proc)) != NULL) {
        if (!isdigit(dent->d_name[0])) continue;
        pid_t pid = atoi(dent->d_name);

        char proc_name[256] = {0};
        load_proc_name(pid, proc_name, sizeof(proc_name));

        if (pattern && *pattern) {
            // Filter by pid or process name substring (case-insensitive)
            int match = 0;
            if (strstr(proc_name, pattern)) match = 1;
            else {
                char pidstr[32];
                snprintf(pidstr, sizeof(pidstr), "%d", pid);
                if (strstr(pidstr, pattern)) match = 1;
            }
            if (!match) continue;
        }

        // List fd entries
        char fd_dir[PATH_MAX];
        snprintf(fd_dir, sizeof(fd_dir), "/proc/%d/fd", pid);
        DIR *fdp = opendir(fd_dir);
        if (!fdp) continue;
        struct dirent *fdent;
        while ((fdent = readdir(fdp)) != NULL) {
            if (fdent->d_name[0] == '.') continue;
            int fd = atoi(fdent->d_name);
            unsigned long long inode;
            if (get_socket_inode_from_fd(pid, fd, &inode) == 0) {
                if (entry_count >= MAX_ENTRIES) {
                    closedir(fdp);
                    closedir(proc);
                    printf("Too many entries, truncated\n");
                    return;
                }
                SocketEntry *e = &entries[entry_count];
                e->pid = pid;
                e->fd = fd;
                strncpy(e->proc_name, proc_name, sizeof(e->proc_name)-1);
                e->proc_name[sizeof(e->proc_name)-1] = 0;
                if (get_remote_addr_from_inode(pid, inode, e->rem_addr, sizeof(e->rem_addr), &e->rem_port) == 0) {
                    // Success, leave rem_addr, rem_port set
                } else {
                    strncpy(e->rem_addr, "?", sizeof(e->rem_addr));
                    e->rem_port = 0;
                }
                entry_count++;
            }
        }
        closedir(fdp);
    }
    closedir(proc);

    printf("Found %d socket(s):\n", entry_count);
    for (int i=0; i<entry_count; i++) {
        SocketEntry *e = &entries[i];
        printf("[%d] PID=%d (%s) FD=%d -> %s:%d\n", i, e->pid, e->proc_name, e->fd, e->rem_addr, e->rem_port);
    }
}

int dup_socket_and_send(int pid, int fd, const char *data, size_t datalen) {
    if (pid <= 0 || fd < 0) {
        fprintf(stderr, "Invalid pid/fd\n");
        return -1;
    }
    int pidfd = pidfd_open(pid, 0);
    if (pidfd < 0) {
        perror("pidfd_open");
        return -1;
    }
    int sockfd = pidfd_getfd(pidfd, fd, 0);
    if (sockfd < 0) {
        perror("pidfd_getfd");
        close(pidfd);
        return -1;
    }
    ssize_t sent = send(sockfd, data, datalen, 0);
    if (sent < 0) perror("send");
    close(sockfd);
    close(pidfd);
    return sent;
}

int dup_socket_and_sendfile(int pid, int fd, const char *filepath) {
    int f = open(filepath, O_RDONLY);
    if (f < 0) {
        perror("open file");
        return -1;
    }
    int pidfd = pidfd_open(pid, 0);
    if (pidfd < 0) {
        perror("pidfd_open");
        close(f);
        return -1;
    }
    int sockfd = pidfd_getfd(pidfd, fd, 0);
    if (sockfd < 0) {
        perror("pidfd_getfd");
        close(f);
        close(pidfd);
        return -1;
    }
    char buf[4096];
    ssize_t n;
    ssize_t total_sent = 0;
    while ((n = read(f, buf, sizeof(buf))) > 0) {
        ssize_t sent = send(sockfd, buf, n, 0);
        if (sent < 0) {
            perror("send");
            close(f);
            close(sockfd);
            close(pidfd);
            return -1;
        }
        total_sent += sent;
    }
    close(f);
    close(sockfd);
    close(pidfd);
    return total_sent;
}

int dup_socket_and_recv(int pid, int fd, const char *outfile) {
    int pidfd = pidfd_open(pid, 0);
    if (pidfd < 0) {
        perror("pidfd_open");
        return -1;
    }
    int sockfd = pidfd_getfd(pidfd, fd, 0);
    if (sockfd < 0) {
        perror("pidfd_getfd");
        close(pidfd);
        return -1;
    }

    fd_set readfds;
    struct timeval tv;
    tv.tv_sec = recv_timeout_sec;
    tv.tv_usec = 0;

    FILE *outf = NULL;
    if (outfile) {
        outf = fopen(outfile, "wb");
        if (!outf) {
            perror("fopen output");
            close(sockfd);
            close(pidfd);
            return -1;
        }
    }

    ssize_t total_received = 0;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        int rv = select(sockfd + 1, &readfds, NULL, NULL, &tv);
        if (rv == 0) {
            printf("Timeout expired (%d seconds)\n", recv_timeout_sec);
            break;
        } else if (rv < 0) {
            perror("select");
            break;
        }
        if (FD_ISSET(sockfd, &readfds)) {
            char buf[4096];
            ssize_t n = recv(sockfd, buf, sizeof(buf), 0);
            if (n < 0) {
                perror("recv");
                break;
            } else if (n == 0) {
                printf("Connection closed by peer\n");
                break;
            } else {
                total_received += n;
                if (outf) {
                    fwrite(buf, 1, n, outf);
                    fflush(outf);
                } else {
                    fwrite(buf, 1, n, stdout);
                    fflush(stdout);
                }
                tv.tv_sec = recv_timeout_sec;
                tv.tv_usec = 0;
            }
        }
    }
    if (outf) fclose(outf);
    close(sockfd);
    close(pidfd);

    printf("Received %zd bytes\n", total_received);
    return 0;
}

void print_usage() {
    printf(
        "Usage:\n"
        "  %s [options]\n"
        "Options:\n"
        "  -p, --pid PID           Specify PID\n"
        "  -s, --socket FD         Specify socket fd\n"
        "  -S, --send STRING       Send string to socket\n"
        "  -F, --sendf FILE        Send file content to socket\n"
        "  -r, --rec [FILE]        Receive from socket, output to stdout or FILE if specified\n"
        "  search [pattern]        Search sockets optionally filtering by pattern\n"
        "  -h, --help              Show this help\n"
        "\n"
        "If no arguments are provided, starts interactive shell.\n",
        "program"
    );
}
// ... ici tu mets toutes les fonctions copiées du js5.c sauf main() et le shell interactif ...
// (tu copies tout de trim_newline à dup_socket_and_recv, y compris les helpers, et adapte les "static" si besoin)
