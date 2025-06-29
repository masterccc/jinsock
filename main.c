#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include "jinsock.h"

int selected_fd = -1;
int recv_timeout_sec = 5;
  
int main(int argc, char *argv[]) {
    if (argc > 1) {
        // Gestion mode ligne de commande
        static struct option long_options[] = {
            {"pid", required_argument, 0, 'p'},
            {"socket", required_argument, 0, 's'},
            {"send", required_argument, 0, 'S'},
            {"sendf", required_argument, 0, 'F'},
            {"rec", optional_argument, 0, 'r'},
            {"help", no_argument, 0, 'h'},
            {0,0,0,0}
        };

        pid_t pid = -1;
        int sockfd = -1;
        char *send_str = NULL;
        char *sendf_file = NULL;
        char *rec_file = NULL;
        int opt;
        int option_index = 0;

        // On vérifie si le premier argument est "search"
        if (strcmp(argv[1], "search") == 0) {
            // Recherche avec ou sans pattern
            if (argc == 2) {
                cmd_search(NULL);
            } else {
                cmd_search(argv[2]);
            }
            return 0;
        }

        // Parsing options getopt_long
        while ((opt = getopt_long(argc, argv, "p:s:S:F:r::h", long_options, &option_index)) != -1) {
            switch (opt) {
                case 'p':
                    pid = atoi(optarg);
                    break;
                case 's':
                    sockfd = atoi(optarg);
                    break;
                case 'S':
                    send_str = optarg;
                    break;
                case 'F':
                    sendf_file = optarg;
                    break;
                case 'r':
                    // --rec peut avoir argument optionnel
                    if (optind < argc && argv[optind][0] != '-') {
                        rec_file = argv[optind];
                        optind++;
                    }
                    else {
                        rec_file = NULL;
                    }
                    break;
                case 'h':
                    print_usage();
                    return 0;
                default:
                    print_usage();
                    return 1;
            }
        }

        // Validation arguments
        int action_count = 0;
        if (send_str) action_count++;
        if (sendf_file) action_count++;
        if (rec_file || (optind > 0 && strcmp(argv[optind-1], "-r") == 0)) action_count++; // rec detected
        if (action_count == 0) {
            fprintf(stderr, "Error: Must specify one action among --send, --sendf, or --rec\n");
            print_usage();
            return 1;
        }
        if (pid <= 0 || sockfd < 0) {
            fprintf(stderr, "Error: Must specify valid --pid and --socket\n");
            print_usage();
            return 1;
        }
        // Execute action
        if (send_str) {
            ssize_t sent = dup_socket_and_send(pid, sockfd, send_str, strlen(send_str));
            if (sent >= 0) {
                printf("Data sent: %zd bytes\n", sent);
                return 0;
            }
            return 1;
        } else if (sendf_file) {
            ssize_t sent = dup_socket_and_sendfile(pid, sockfd, sendf_file);
            if (sent >= 0) {
                printf("File sent: %zd bytes\n", sent);
                return 0;
            }
            return 1;
        } else {
            ssize_t ret = dup_socket_and_recv(pid, sockfd, rec_file);
            return (ret == 0) ? 0 : 1;
        }
    }

    char line[1024];
    printf("Socket Injector Shell. Type 'help' for commands.\n");
    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        trim_newline(line);

        if (strncmp(line, "help", 4) == 0) {
            cmd_help();
        } else if (strncmp(line, "search", 6) == 0) {
            char *arg = line + 6;
            while (*arg == ' ') arg++;
            cmd_search(*arg ? arg : NULL);
        } else if (strncmp(line, "select", 6) == 0) {
            int idx = -1;
            if (sscanf(line + 6, "%d", &idx) != 1 || idx < 0 || idx >= entry_count) {
                printf("Invalid index\n");
            } else {
                selected_fd = idx;
                printf("Selected entry [%d]\n", idx);
            }
        } else if (strncmp(line, "sendf", 5) == 0) {
            if (selected_fd < 0) {
                printf("No socket selected\n");
                continue;
            }
            char *filename = line + 5;
            while (*filename == ' ') filename++;
            if (*filename == 0) {
                printf("Missing filename\n");
                continue;
            }
            SocketEntry *e = &entries[selected_fd];
            ssize_t sent = dup_socket_and_sendfile(e->pid, e->fd, filename);
            if (sent >= 0)
                printf("File sent: %zd bytes\n", sent);
        } else if (strncmp(line, "send", 4) == 0) {
            if (selected_fd < 0) {
                printf("No socket selected\n");
                continue;
            }
            char *data = line + 4;
            while (*data == ' ') data++;
            if (*data == 0) {
                printf("Missing data to send\n");
                continue;
            }
            SocketEntry *e = &entries[selected_fd];
            ssize_t sent = dup_socket_and_send(e->pid, e->fd, data, strlen(data));
            if (sent >= 0)
                printf("Data sent: %zd bytes\n", sent);
        } else if (strncmp(line, "rec", 3) == 0) {
            if (selected_fd < 0) {
                printf("No socket selected\n");
                continue;
            }
            char *filename = line + 3;
            while (*filename == ' ') filename++;
            SocketEntry *e = &entries[selected_fd];
            if (*filename)
                dup_socket_and_recv(e->pid, e->fd, filename);
            else
                dup_socket_and_recv(e->pid, e->fd, NULL);
        } else if (strncmp(line, "timeout", 7) == 0) {
            int t = 0;
            if (sscanf(line + 7, "%d", &t) != 1 || t <= 0) {
                printf("Invalid timeout value\n");
            } else {
                recv_timeout_sec = t;
                printf("Timeout set to %d seconds\n", recv_timeout_sec);
            }
        } else if (strncmp(line, "quit", 4) == 0) {
            break;
        } else if (strlen(line) == 0) {
            // empty, ignore
        } else {
            printf("Unknown command: %s\n", line);
        }
    }

    printf("Bye.\n");
    return 0;
}
