/*
 * Dust Init - PID 1
 * Sistema init leggero e moderno per Linux
 * Versione: 0.1.0
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/reboot.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>
#include <stdarg.h>
#include <dirent.h>

#define VERSION "0.1.0"
#define DUST_CONF_DIR "/etc/dust"
#define DUST_SVC_DIR "/etc/dust/services"
#define DUST_TARGET_DIR "/etc/dust/targets"
#define DUST_RUN_DIR "/run/dust"
#define DUST_LOG_FILE "/var/log/dust.log"

/* Colori */
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define RESET "\033[0m"
#define BOLD "\033[1m"

/* Livelli di log */
typedef enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL } log_level_t;

/* Stati del sistema */
typedef enum { STATE_BOOTING, STATE_RUNNING, STATE_STOPPING, STATE_REBOOTING, STATE_HALTING } system_state_t;

/* Struttura servizio */
typedef struct service {
    char name[256];
    char exec_start[512];
    char exec_stop[512];
    char exec_reload[512];
    char working_dir[256];
    char user[64];
    char group[64];
    char type[32];
    char restart_policy[32];
    char after[512];
    char wants[512];
    char wanted_by[256];
    char description[512];
    char environment[1024];
    int restart_sec;
    int timeout_start;
    int timeout_stop;
    int max_restarts;
    int nice_level;
    pid_t pid;
    int restart_count;
    int enabled;
    int active;
    int failed;
    time_t start_time;
    time_t stop_time;
    struct service *next;
} service_t;

/* Struttura target */
typedef struct target {
    char name[256];
    char description[512];
    char requires[512];
    char wants[512];
    char conflicts[512];
    char after[512];
    char before[512];
    int active;
    struct target *next;
} target_t;

/* Variabili globali */
static volatile sig_atomic_t got_signal = 0;
static volatile sig_atomic_t got_sigchld = 0;
static system_state_t system_state = STATE_BOOTING;
static service_t *services = NULL;
static target_t *targets = NULL;
static target_t *current_target = NULL;
static int debug_mode = 0;
static int dry_run = 0;

/* Prototipi */
static void log_message(log_level_t level, const char *fmt, ...);
static void signal_handler(int sig);
static void setup_signals(void);
static void reap_children(void);
static int mount_filesystems(void);
static int setup_devfs(void);
static int parse_service_file(const char *filename, service_t *svc);
static int parse_target_file(const char *filename, target_t *tgt);
static int load_services(void);
static int load_targets(void);
static int start_service(service_t *svc);
static int stop_service(service_t *svc);
static service_t* find_service(const char *name);
static target_t* find_target(const char *name);
static int switch_target(target_t *tgt);
static int start_default_target(void);
static void main_loop(void);
static void cleanup_and_shutdown(void);

/* Logging */
static void log_message(log_level_t level, const char *fmt, ...) {
    va_list args;
    time_t now;
    struct tm *tm_info;
    char time_str[26];
    const char *level_str[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    const char *level_color[] = {CYAN, GREEN, YELLOW, RED, MAGENTA};
    
    if (level == LOG_DEBUG && !debug_mode) return;
    
    time(&now);
    tm_info = localtime(&now);
    strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    
    FILE *log_fp = NULL;
    if (!dry_run) {
        log_fp = fopen(DUST_LOG_FILE, "a");
    }
    
    if (isatty(STDOUT_FILENO)) {
        printf("%s[%s]%s %s%-5s%s ", 
               BOLD, time_str, RESET,
               level_color[level], level_str[level], RESET);
    } else {
        printf("[%s] %-5s ", time_str, level_str[level]);
    }
    
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    
    if (log_fp) {
        fprintf(log_fp, "[%s] %-5s ", time_str, level_str[level]);
        va_start(args, fmt);
        vfprintf(log_fp, fmt, args);
        va_end(args);
        fprintf(log_fp, "\n");
        fclose(log_fp);
    }
    
    if (level == LOG_FATAL) {
        cleanup_and_shutdown();
        exit(EXIT_FAILURE);
    }
}

/* Signal handling */
static void signal_handler(int sig) {
    if (sig == SIGCHLD) {
        got_sigchld = 1;
    } else {
        got_signal = sig;
    }
}

static void setup_signals(void) {
    struct sigaction sa;
    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    sigaction(SIGCHLD, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    
    signal(SIGPIPE, SIG_IGN);
}

/* Reap zombie processes */
static void reap_children(void) {
    pid_t pid;
    int status;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        service_t *svc = services;
        while (svc) {
            if (svc->pid == pid) {
                svc->pid = 0;
                svc->active = 0;
                svc->stop_time = time(NULL);
                
                if (WIFEXITED(status)) {
                    int exit_code = WEXITSTATUS(status);
                    if (exit_code != 0) {
                        svc->failed = 1;
                        log_message(LOG_ERROR, "Service %s exited with code %d", svc->name, exit_code);
                    } else {
                        log_message(LOG_INFO, "Service %s stopped normally", svc->name);
                    }
                } else if (WIFSIGNALED(status)) {
                    int sig = WTERMSIG(status);
                    svc->failed = 1;
                    log_message(LOG_ERROR, "Service %s killed by signal %d", svc->name, sig);
                }
                
                /* Handle restart policy */
                if (svc->enabled && system_state == STATE_RUNNING) {
                    int should_restart = 0;
                    
                    if (strcmp(svc->restart_policy, "always") == 0) {
                        should_restart = 1;
                    } else if (strcmp(svc->restart_policy, "on-failure") == 0 && svc->failed) {
                        should_restart = 1;
                    } else if (strcmp(svc->restart_policy, "on-success") == 0 && !svc->failed) {
                        should_restart = 1;
                    }
                    
                    if (should_restart && svc->restart_count < svc->max_restarts) {
                        log_message(LOG_INFO, "Restarting service %s (attempt %d/%d)", 
                                   svc->name, svc->restart_count + 1, svc->max_restarts);
                        svc->restart_count++;
                        sleep(svc->restart_sec);
                        start_service(svc);
                    } else if (svc->restart_count >= svc->max_restarts) {
                        log_message(LOG_ERROR, "Service %s exceeded max restarts (%d)", 
                                   svc->name, svc->max_restarts);
                    }
                }
                break;
            }
            svc = svc->next;
        }
    }
}

/* Mount filesystems */
static int mount_filesystems(void) {
    log_message(LOG_INFO, "Mounting essential filesystems...");
    
    if (mount("none", "/proc", "proc", MS_NOEXEC | MS_NOSUID | MS_NODEV, NULL) < 0) {
        if (errno != EBUSY) {
            log_message(LOG_WARN, "Failed to mount /proc: %s", strerror(errno));
        }
    }
    
    if (mount("none", "/sys", "sysfs", MS_NOEXEC | MS_NOSUID | MS_NODEV, NULL) < 0) {
        if (errno != EBUSY) {
            log_message(LOG_WARN, "Failed to mount /sys: %s", strerror(errno));
        }
    }
    
    if (mount("none", "/dev", "devtmpfs", MS_NOSUID | MS_STRICTATIME, "mode=0755")) {
        if (errno != EBUSY) {
            log_message(LOG_WARN, "Failed to mount /dev: %s", strerror(errno));
            if (mount("none", "/dev", "tmpfs", MS_NOSUID | MS_STRICTATIME, "mode=0755")) {
                if (errno != EBUSY) {
                    log_message(LOG_ERROR, "Failed to mount /dev even as tmpfs: %s", strerror(errno));
                }
            }
        }
    }
    
    if (mount("none", "/run", "tmpfs", MS_NOSUID | MS_NODEV | MS_STRICTATIME, "mode=0755")) {
        if (errno != EBUSY) {
            log_message(LOG_WARN, "Failed to mount /run: %s", strerror(errno));
        }
    }
    
    /* Create directories */
    mkdir("/run/dust", 0755);
    mkdir("/var/log", 0755);
    mkdir("/etc/dust", 0755);
    mkdir("/etc/dust/services", 0755);
    mkdir("/etc/dust/targets", 0755);
    
    log_message(LOG_INFO, "Filesystems mounted successfully");
    return 0;
}

/* Setup device filesystem */
static int setup_devfs(void) {
    log_message(LOG_INFO, "Setting up device nodes...");
    
    if (access("/dev/null", F_OK) != 0) {
        mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3));
    }
    if (access("/dev/zero", F_OK) != 0) {
        mknod("/dev/zero", S_IFCHR | 0666, makedev(1, 5));
    }
    if (access("/dev/random", F_OK) != 0) {
        mknod("/dev/random", S_IFCHR | 0666, makedev(1, 8));
    }
    if (access("/dev/urandom", F_OK) != 0) {
        mknod("/dev/urandom", S_IFCHR | 0666, makedev(1, 9));
    }
    if (access("/dev/console", F_OK) != 0) {
        mknod("/dev/console", S_IFCHR | 0600, makedev(5, 1));
    }
    if (access("/dev/tty", F_OK) != 0) {
        mknod("/dev/tty", S_IFCHR | 0666, makedev(5, 0));
    }
    
    symlink("/proc/self/fd", "/dev/fd");
    symlink("/proc/self/fd/0", "/dev/stdin");
    symlink("/proc/self/fd/1", "/dev/stdout");
    symlink("/proc/self/fd/2", "/dev/stderr");
    
    log_message(LOG_INFO, "Device nodes setup complete");
    return 0;
}

/* Parse service file */
static int parse_service_file(const char *filename, service_t *svc) {
    FILE *fp;
    char line[1024];
    char section[64] = "";
    
    memset(svc, 0, sizeof(service_t));
    
    strcpy(svc->type, "simple");
    strcpy(svc->restart_policy, "no");
    svc->restart_sec = 5;
    svc->timeout_start = 60;
    svc->timeout_stop = 30;
    svc->max_restarts = 5;
    svc->nice_level = 0;
    
    const char *base = strrchr(filename, '/');
    if (base) base++;
    else base = filename;
    strncpy(svc->name, base, sizeof(svc->name) - 1);
    char *dot = strrchr(svc->name, '.');
    if (dot) *dot = '\0';
    
    fp = fopen(filename, "r");
    if (!fp) return -1;
    
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '\0' || *trimmed == '#') continue;
        
        if (trimmed[0] == '[') {
            char *end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                strncpy(section, trimmed + 1, sizeof(section) - 1);
            }
            continue;
        }
        
        char *equal = strchr(trimmed, '=');
        if (!equal) continue;
        
        *equal = '\0';
        char *key = trimmed;
        char *value = equal + 1;
        
        while (*key == ' ' || *key == '\t') key++;
        while (*value == ' ' || *value == '\t') value++;
        
        if (strcmp(section, "Unit") == 0) {
            if (strcmp(key, "Description") == 0) strncpy(svc->description, value, sizeof(svc->description) - 1);
            else if (strcmp(key, "After") == 0) strncpy(svc->after, value, sizeof(svc->after) - 1);
            else if (strcmp(key, "Wants") == 0) strncpy(svc->wants, value, sizeof(svc->wants) - 1);
        } else if (strcmp(section, "Service") == 0) {
            if (strcmp(key, "Type") == 0) strncpy(svc->type, value, sizeof(svc->type) - 1);
            else if (strcmp(key, "ExecStart") == 0) strncpy(svc->exec_start, value, sizeof(svc->exec_start) - 1);
            else if (strcmp(key, "ExecStop") == 0) strncpy(svc->exec_stop, value, sizeof(svc->exec_stop) - 1);
            else if (strcmp(key, "ExecReload") == 0) strncpy(svc->exec_reload, value, sizeof(svc->exec_reload) - 1);
            else if (strcmp(key, "WorkingDirectory") == 0) strncpy(svc->working_dir, value, sizeof(svc->working_dir) - 1);
            else if (strcmp(key, "User") == 0) strncpy(svc->user, value, sizeof(svc->user) - 1);
            else if (strcmp(key, "Group") == 0) strncpy(svc->group, value, sizeof(svc->group) - 1);
            else if (strcmp(key, "Restart") == 0) strncpy(svc->restart_policy, value, sizeof(svc->restart_policy) - 1);
            else if (strcmp(key, "RestartSec") == 0) svc->restart_sec = atoi(value);
            else if (strcmp(key, "TimeoutStartSec") == 0) svc->timeout_start = atoi(value);
            else if (strcmp(key, "TimeoutStopSec") == 0) svc->timeout_stop = atoi(value);
            else if (strcmp(key, "StartLimitBurst") == 0) svc->max_restarts = atoi(value);
            else if (strcmp(key, "Nice") == 0) svc->nice_level = atoi(value);
            else if (strcmp(key, "Environment") == 0) {
                strncat(svc->environment, value, sizeof(svc->environment) - strlen(svc->environment) - 1);
                strncat(svc->environment, " ", sizeof(svc->environment) - strlen(svc->environment) - 1);
            }
        } else if (strcmp(section, "Install") == 0) {
            if (strcmp(key, "WantedBy") == 0) strncpy(svc->wanted_by, value, sizeof(svc->wanted_by) - 1);
        }
    }
    
    fclose(fp);
    return 0;
}

/* Parse target file */
static int parse_target_file(const char *filename, target_t *tgt) {
    FILE *fp;
    char line[1024];
    char section[64] = "";
    
    memset(tgt, 0, sizeof(target_t));
    
    const char *base = strrchr(filename, '/');
    if (base) base++;
    else base = filename;
    strncpy(tgt->name, base, sizeof(tgt->name) - 1);
    char *dot = strrchr(tgt->name, '.');
    if (dot) *dot = '\0';
    
    fp = fopen(filename, "r");
    if (!fp) return -1;
    
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '\0' || *trimmed == '#') continue;
        
        if (trimmed[0] == '[') {
            char *end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                strncpy(section, trimmed + 1, sizeof(section) - 1);
            }
            continue;
        }
        
        char *equal = strchr(trimmed, '=');
        if (!equal) continue;
        
        *equal = '\0';
        char *key = trimmed;
        char *value = equal + 1;
        
        while (*key == ' ' || *key == '\t') key++;
        while (*value == ' ' || *value == '\t') value++;
        
        if (strcmp(section, "Unit") == 0) {
            if (strcmp(key, "Description") == 0) strncpy(tgt->description, value, sizeof(tgt->description) - 1);
            else if (strcmp(key, "Requires") == 0) strncpy(tgt->requires, value, sizeof(tgt->requires) - 1);
            else if (strcmp(key, "Wants") == 0) strncpy(tgt->wants, value, sizeof(tgt->wants) - 1);
            else if (strcmp(key, "Conflicts") == 0) strncpy(tgt->conflicts, value, sizeof(tgt->conflicts) - 1);
            else if (strcmp(key, "After") == 0) strncpy(tgt->after, value, sizeof(tgt->after) - 1);
            else if (strcmp(key, "Before") == 0) strncpy(tgt->before, value, sizeof(tgt->before) - 1);
        }
    }
    
    fclose(fp);
    return 0;
}

/* Find service */
static service_t* find_service(const char *name) {
    service_t *svc = services;
    while (svc) {
        if (strcmp(svc->name, name) == 0) return svc;
        svc = svc->next;
    }
    return NULL;
}

/* Find target */
static target_t* find_target(const char *name) {
    target_t *tgt = targets;
    while (tgt) {
        if (strcmp(tgt->name, name) == 0) return tgt;
        tgt = tgt->next;
    }
    return NULL;
}

/* Start service */
static int start_service(service_t *svc) {
    if (!svc || svc->active) return -1;
    
    log_message(LOG_INFO, "Starting service: %s", svc->name);
    
    if (dry_run) {
        log_message(LOG_INFO, "[DRY RUN] Would start: %s", svc->exec_start);
        return 0;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        log_message(LOG_ERROR, "Failed to fork for service %s: %s", svc->name, strerror(errno));
        return -1;
    }
    
    if (pid == 0) {
        /* Child process */
        if (svc->environment[0]) {
            char *env = strdup(svc->environment);
            char *token = strtok(env, " ");
            while (token) {
                putenv(token);
                token = strtok(NULL, " ");
            }
            free(env);
        }
        
        if (svc->working_dir[0]) {
            chdir(svc->working_dir);
        }
        
        if (svc->nice_level != 0) {
            nice(svc->nice_level);
        }
        
        char *args[64];
        char *cmd = strdup(svc->exec_start);
        int i = 0;
        char *token = strtok(cmd, " ");
        while (token && i < 63) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;
        
        execvp(args[0], args);
        log_message(LOG_ERROR, "Failed to execute %s: %s", args[0], strerror(errno));
        _exit(EXIT_FAILURE);
    }
    
    /* Parent process */
    svc->pid = pid;
    svc->active = 1;
    svc->failed = 0;
    svc->start_time = time(NULL);
    svc->restart_count = 0;
    
    log_message(LOG_INFO, "Service %s started with PID %d", svc->name, pid);
    return 0;
}

/* Stop service */
static int stop_service(service_t *svc) {
    if (!svc || !svc->active || svc->pid <= 0) return -1;
    
    log_message(LOG_INFO, "Stopping service: %s (PID %d)", svc->name, svc->pid);
    
    if (dry_run) {
        log_message(LOG_INFO, "[DRY RUN] Would stop: %s", svc->name);
        return 0;
    }
    
    if (svc->exec_stop[0]) {
        int ret = system(svc->exec_stop);
        if (ret == 0) {
            int timeout = svc->timeout_stop;
            while (svc->pid > 0 && timeout > 0) {
                usleep(100000);
                timeout -= 100;
            }
        }
    } else {
        kill(svc->pid, SIGTERM);
        int timeout = svc->timeout_stop;
        while (svc->pid > 0 && timeout > 0) {
            usleep(100000);
            timeout -= 100;
        }
    }
    
    if (svc->pid > 0) {
        log_message(LOG_WARN, "Service %s did not stop gracefully, forcing...", svc->name);
        kill(svc->pid, SIGKILL);
        int timeout = 50;
        while (svc->pid > 0 && timeout > 0) {
            usleep(100000);
            timeout -= 100;
        }
    }
    
    svc->active = 0;
    svc->stop_time = time(NULL);
    
    log_message(LOG_INFO, "Service %s stopped", svc->name);
    return 0;
}

/* Load services */
static int load_services(void) {
    DIR *dir;
    struct dirent *entry;
    
    log_message(LOG_INFO, "Loading services from %s", DUST_SVC_DIR);
    
    dir = opendir(DUST_SVC_DIR);
    if (!dir) {
        log_message(LOG_WARN, "Cannot open services directory: %s", strerror(errno));
        return -1;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char *ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".yaml") == 0) {
                service_t *svc = malloc(sizeof(service_t));
                if (svc) {
                    char path[512];
                    snprintf(path, sizeof(path), "%s/%s", DUST_SVC_DIR, entry->d_name);
                    
                    if (parse_service_file(path, svc) == 0) {
                        svc->next = services;
                        services = svc;
                        log_message(LOG_DEBUG, "Loaded service: %s", svc->name);
                    } else {
                        free(svc);
                    }
                }
            }
        }
    }
    
    closedir(dir);
    log_message(LOG_INFO, "Services loaded successfully");
    return 0;
}

/* Load targets */
static int load_targets(void) {
    DIR *dir;
    struct dirent *entry;
    
    log_message(LOG_INFO, "Loading targets from %s", DUST_TARGET_DIR);
    
    dir = opendir(DUST_TARGET_DIR);
    if (!dir) {
        log_message(LOG_WARN, "Cannot open targets directory: %s", strerror(errno));
        return -1;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char *ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".yaml") == 0) {
                target_t *tgt = malloc(sizeof(target_t));
                if (tgt) {
                    char path[512];
                    snprintf(path, sizeof(path), "%s/%s", DUST_TARGET_DIR, entry->d_name);
                    
                    if (parse_target_file(path, tgt) == 0) {
                        tgt->next = targets;
                        targets = tgt;
                        log_message(LOG_DEBUG, "Loaded target: %s", tgt->name);
                    } else {
                        free(tgt);
                    }
                }
            }
        }
    }
    
    closedir(dir);
    log_message(LOG_INFO, "Targets loaded successfully");
    return 0;
}

/* Start default target */
static int start_default_target(void) {
    target_t *def = find_target("default");
    if (!def) def = find_target("multi-user");
    if (!def) def = targets;
    
    if (def) return switch_target(def);
    return -1;
}

/* Switch target */
static int switch_target(target_t *tgt) {
    if (!tgt) return -1;
    
    log_message(LOG_INFO, "Switching to target: %s", tgt->name);
    
    if (current_target) current_target->active = 0;
    
    tgt->active = 1;
    current_target = tgt;
    
    service_t *svc = services;
    while (svc) {
        if (svc->enabled && strstr(svc->wanted_by, tgt->name)) {
            if (!svc->active) start_service(svc);
        }
        svc = svc->next;
    }
    
    return 0;
}

/* Main loop */
static void main_loop(void) {
    system_state = STATE_RUNNING;
    
    log_message(LOG_INFO, "Entering main loop");
    
    while (system_state == STATE_RUNNING) {
        if (got_sigchld) {
            got_sigchld = 0;
            reap_children();
        }
        
        if (got_signal) {
            int sig = got_signal;
            got_signal = 0;
            
            switch (sig) {
                case SIGTERM:
                case SIGINT:
                    log_message(LOG_INFO, "Received signal %d, shutting down...", sig);
                    system_state = STATE_STOPPING;
                    break;
                case SIGHUP:
                    log_message(LOG_INFO, "Received SIGHUP, reloading configuration...");
                    load_services();
                    load_targets();
                    break;
                case SIGUSR1:
                    log_message(LOG_INFO, "Received SIGUSR1, reopening log files...");
                    break;
                case SIGUSR2:
                    log_message(LOG_INFO, "Received SIGUSR2, toggling debug mode...");
                    debug_mode = !debug_mode;
                    break;
            }
        }
        
        usleep(100000);
    }
    
    log_message(LOG_INFO, "Exiting main loop");
}

/* Cleanup */
static void cleanup_and_shutdown(void) {
    log_message(LOG_INFO, "Shutting down Dust...");
    
    service_t *svc = services;
    while (svc) {
        if (svc->active && svc->pid > 0) {
            log_message(LOG_INFO, "Stopping service %s", svc->name);
            stop_service(svc);
        }
        svc = svc->next;
    }
    
    sync();
}

/* Helper functions */
static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("\nDust Init - A lightweight init system for Linux\n\n");
    printf("Options:\n");
    printf("  -h, --help       Show this help message\n");
    printf("  -v, --version    Show version information\n");
    printf("  -d, --debug      Enable debug mode\n");
    printf("  -D, --dry-run    Dry run mode\n");
    printf("\n");
}

static void print_version(void) {
    printf("Dust Init version %s\n", VERSION);
    printf("Copyright (C) 2024 Dust Team\n");
}

static int is_running_as_pid1(void) {
    return getpid() == 1;
}

/* Main entry point */
int main(int argc, char *argv[]) {
    int opt;
    int init_mode = 0;
    
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"debug", no_argument, 0, 'd'},
        {"dry-run", no_argument, 0, 'D'},
        {"init", no_argument, 0, 1},
        {0, 0, 0, 0}
    };
    
    if (is_running_as_pid1()) init_mode = 1;
    
    while ((opt = getopt_long(argc, argv, "hvdD", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h': print_usage(argv[0]); return 0;
            case 'v': print_version(); return 0;
            case 'd': debug_mode = 1; break;
            case 'D': dry_run = 1; break;
            case 1: init_mode = 1; break;
            default: print_usage(argv[0]); return 1;
        }
    }
    
    setup_signals();
    
    if (init_mode) {
        log_message(LOG_INFO, "========================================");
        log_message(LOG_INFO, "Dust Init v%s starting as PID 1", VERSION);
        log_message(LOG_INFO, "========================================");
        
        if (mount_filesystems() < 0) {
            log_message(LOG_FATAL, "Failed to mount essential filesystems");
        }
        
        setup_devfs();
        load_services();
        load_targets();
        start_default_target();
        main_loop();
        cleanup_and_shutdown();
    } else {
        log_message(LOG_INFO, "Dust Init running in service manager mode");
        load_services();
        load_targets();
        main_loop();
    }
    
    return 0;
}
