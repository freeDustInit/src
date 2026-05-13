/*
 * Dust CLI - Command Line Interface
 * Client per la gestione dei servizi e del sistema
 * Versione: 0.1.0
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <time.h>

#define VERSION "0.1.0"
#define DUST_SOCKET "/run/dust/dust.sock"

/* Colori */
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define RESET "\033[0m"
#define BOLD "\033[1m"

/* Tipi di messaggi */
typedef enum {
    CMD_START,
    CMD_STOP,
    CMD_RESTART,
    CMD_STATUS,
    CMD_ENABLE,
    CMD_DISABLE,
    CMD_LIST,
    CMD_LOGS,
    CMD_DAEMON_RELOAD
} command_type_t;

/* Struttura messaggio */
typedef struct {
    command_type_t cmd;
    char service[256];
    char args[1024];
} dust_message_t;

/* Prototipi */
static void print_usage(const char *prog);
static void print_version(void);
static void print_help(void);
static int send_command(command_type_t cmd, const char *service, const char *args);
static int connect_to_daemon(void);
static void print_colored_status(const char *service, const char *status);

/* Usage */
static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS] COMMAND [SERVICE]\n", prog);
    printf("       %s --version\n", prog);
    printf("       %s --help\n", prog);
    printf("\nRun '%s help' for more information.\n", prog);
}

/* Version */
static void print_version(void) {
    printf("dust version %s\n", VERSION);
    printf("Copyright (C) 2024 Dust Team\n");
    printf("License MIT: The MIT License <https://opensource.org/licenses/MIT>\n");
    printf("This is free software: you are free to change and redistribute it.\n");
    printf("There is NO WARRANTY, to the extent permitted by law.\n");
}

/* Help */
static void print_help(void) {
    printf("%sDust - Lightweight Init System for Linux%s\n\n", BOLD, RESET);
    
    printf("%sUsage:%s\n", BOLD, RESET);
    printf("  dust [OPTIONS] COMMAND [SERVICE]\n\n");
    
    printf("%sOptions:%s\n", BOLD, RESET);
    printf("  -h, --help          Show this help message\n");
    printf("  -v, --version       Show version information\n");
    printf("  -V, --verbose       Enable verbose output\n");
    printf("  --no-pager          Do not pipe output into a pager\n");
    printf("\n");
    
    printf("%sCommands:%s\n", BOLD, RESET);
    printf("  %sstart%s <service>       Start a service\n", GREEN, RESET);
    printf("  %sstop%s <service>        Stop a service\n", RED, RESET);
    printf("  %srestart%s <service>    Restart a service\n", YELLOW, RESET);
    printf("  %sreload%s <service>      Reload service configuration\n", BLUE, RESET);
    printf("  %sstatus%s [service]      Show service status (or all services)\n", CYAN, RESET);
    printf("  %senable%s <service>      Enable service to start at boot\n", GREEN, RESET);
    printf("  %sdisable%s <service>     Disable service from starting at boot\n", RED, RESET);
    printf("  %slist-units%s            List all loaded units\n", MAGENTA, RESET);
    printf("  %sdaemon-reload%s         Reload systemd manager configuration\n", BLUE, RESET);
    printf("\n");
    
    printf("%sTarget Commands:%s\n", BOLD, RESET);
    printf("  %sisolate%s <target>     Change to a specific target\n", YELLOW, RESET);
    printf("  %sget-default%s            Get the default target\n", CYAN, RESET);
    printf("  %sset-default%s <target>   Set the default target\n", GREEN, RESET);
    printf("\n");
    
    printf("%sExamples:%s\n", BOLD, RESET);
    printf("  dust start sshd              # Start the SSH service\n");
    printf("  dust status nginx            # Check nginx status\n");
    printf("  dust enable postgresql         # Enable PostgreSQL at boot\n");
    printf("  dust list-units --state=failed # List failed services\n");
    printf("  dust isolate graphical.target  # Switch to graphical mode\n");
    printf("\n");
    
    printf("%sConfiguration:%s\n", BOLD, RESET);
    printf("  Service files: /etc/dust/services/\n");
    printf("  Target files:  /etc/dust/targets/\n");
    printf("  Runtime data:  /run/dust/\n");
    printf("  Log file:      /var/log/dust.log\n");
    printf("\n");
    
    printf("For more information, visit: https://github.com/yourusername/dust\n");
}

/* Connect to daemon */
static int connect_to_daemon(void) {
    int sock;
    struct sockaddr_un addr;
    
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, DUST_SOCKET, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    return sock;
}

/* Send command */
static int send_command(command_type_t cmd, const char *service, const char *args) {
    int sock;
    dust_message_t msg;
    
    sock = connect_to_daemon();
    if (sock < 0) {
        /* Daemon not running, fallback to direct execution */
        printf("Warning: Dust daemon not running. Using fallback mode.\n");
        return -1;
    }
    
    memset(&msg, 0, sizeof(msg));
    msg.cmd = cmd;
    if (service) {
        strncpy(msg.service, service, sizeof(msg.service) - 1);
    }
    if (args) {
        strncpy(msg.args, args, sizeof(msg.args) - 1);
    }
    
    if (send(sock, &msg, sizeof(msg), 0) < 0) {
        perror("send");
        close(sock);
        return -1;
    }
    
    /* Receive response */
    char response[4096];
    int n = recv(sock, response, sizeof(response) - 1, 0);
    if (n > 0) {
        response[n] = '\0';
        printf("%s", response);
    }
    
    close(sock);
    return 0;
}

/* Print colored status */
static void print_colored_status(const char *service, const char *status) {
    const char *color = RESET;
    
    if (strcmp(status, "active") == 0 || strcmp(status, "running") == 0) {
        color = GREEN;
    } else if (strcmp(status, "inactive") == 0 || strcmp(status, "stopped") == 0) {
        color = RED;
    } else if (strcmp(status, "failed") == 0) {
        color = RED;
    } else if (strcmp(status, "activating") == 0 || strcmp(status, "deactivating") == 0) {
        color = YELLOW;
    }
    
    printf("  %s%-20s%s %s%s%s\n", BOLD, service, RESET, color, status, RESET);
}

/* Main function */
int main(int argc, char *argv[]) {
    int opt;
    int verbose = 0;
    int show_help = 0;
    int show_version = 0;
    
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"verbose", no_argument, 0, 'V'},
        {"no-pager", no_argument, 0, 'P'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "hvVP", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                show_help = 1;
                break;
            case 'v':
                show_version = 1;
                break;
            case 'V':
                verbose = 1;
                break;
            case 'P':
                /* No pager - not implemented yet */
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    if (show_version) {
        print_version();
        return 0;
    }
    
    if (show_help || argc == 1) {
        print_help();
        return 0;
    }
    
    /* Parse command */
    if (optind < argc) {
        const char *command = argv[optind];
        const char *service = (optind + 1 < argc) ? argv[optind + 1] : NULL;
        
        if (strcmp(command, "start") == 0) {
            if (!service) {
                fprintf(stderr, "Error: start requires a service name\n");
                return 1;
            }
            printf("Starting service: %s\n", service);
            return send_command(CMD_START, service, NULL);
            
        } else if (strcmp(command, "stop") == 0) {
            if (!service) {
                fprintf(stderr, "Error: stop requires a service name\n");
                return 1;
            }
            printf("Stopping service: %s\n", service);
            return send_command(CMD_STOP, service, NULL);
            
        } else if (strcmp(command, "restart") == 0) {
            if (!service) {
                fprintf(stderr, "Error: restart requires a service name\n");
                return 1;
            }
            printf("Restarting service: %s\n", service);
            return send_command(CMD_RESTART, service, NULL);
            
        } else if (strcmp(command, "status") == 0) {
            if (service) {
                printf("Status of service: %s\n", service);
            } else {
                printf("Status of all services:\n");
            }
            return send_command(CMD_STATUS, service, NULL);
            
        } else if (strcmp(command, "enable") == 0) {
            if (!service) {
                fprintf(stderr, "Error: enable requires a service name\n");
                return 1;
            }
            printf("Enabling service: %s\n", service);
            return send_command(CMD_ENABLE, service, NULL);
            
        } else if (strcmp(command, "disable") == 0) {
            if (!service) {
                fprintf(stderr, "Error: disable requires a service name\n");
                return 1;
            }
            printf("Disabling service: %s\n", service);
            return send_command(CMD_DISABLE, service, NULL);
            
        } else if (strcmp(command, "list-units") == 0 || strcmp(command, "list") == 0) {
            printf("Listing all units:\n");
            return send_command(CMD_LIST, NULL, NULL);
            
        } else if (strcmp(command, "daemon-reload") == 0) {
            printf("Reloading daemon configuration...\n");
            return send_command(CMD_DAEMON_RELOAD, NULL, NULL);
            
        } else if (strcmp(command, "help") == 0) {
            print_help();
            return 0;
            
        } else {
            fprintf(stderr, "Error: Unknown command '%s'\n", command);
            fprintf(stderr, "Run '%s help' for usage information.\n", argv[0]);
            return 1;
        }
    }
    
    return 0;
}
