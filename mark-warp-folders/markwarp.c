/*
 * markwarp.c - Fast Directory Bookmarking & Instant Warp Utility
 * Features: Mark directories with tags and warp to them instantly from anywhere.
 * Supports: Linux, macOS, Windows, Android (Termux).
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* --- Cross-Platform Absolute Path & OS Layer --- */
#if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS 1
    #include <windows.h>
    #include <io.h>
    static char *get_absolute_path(const char *path, char *out_buf, size_t max_size) {
        return _fullpath(out_buf, path, max_size);
    }
#else
    #define OS_UNIX 1
    #include <unistd.h>
    #include <limits.h>
    static char *get_absolute_path(const char *path, char *out_buf, size_t max_size) {
        (void)max_size;
        return realpath(path, out_buf);
    }
#endif

/* --- TTY & Color Support --- */
static int use_colors = 0;

static void init_tty_check(void) {
#ifdef OS_WINDOWS
    use_colors = _isatty(_fileno(stdout));
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | 0x0004);
        }
    }
#else
    use_colors = isatty(STDOUT_FILENO);
#endif
}

#define C_RESET   (use_colors ? "\033[0m"  : "")
#define C_BOLD    (use_colors ? "\033[1m"  : "")
#define C_RED     (use_colors ? "\033[31m" : "")
#define C_GREEN   (use_colors ? "\033[32m" : "")
#define C_YELLOW  (use_colors ? "\033[33m" : "")
#define C_CYAN    (use_colors ? "\033[36m" : "")

/* --- Get User Home Bookmarks File Path (~/.markwarp_bookmarks) --- */
static void get_config_file_path(char *out_path, size_t max_size) {
    const char *home = NULL;
#ifdef OS_WINDOWS
    home = getenv("USERPROFILE");
    if (!home) home = getenv("HOMEDRIVE");
#else
    home = getenv("HOME");
#endif
    if (!home) home = ".";

    snprintf(out_path, max_size, "%s/.markwarp_bookmarks", home);
}

/* --- COMMAND: mark <tag> [path] --- */
static void cmd_mark(const char *tag, const char *path_arg) {
    char abs_path[1024];
    const char *target = (path_arg && strlen(path_arg) > 0) ? path_arg : ".";

    if (!get_absolute_path(target, abs_path, sizeof(abs_path))) {
        printf("%s[-] Invalid path: %s%s\n", C_RED, target, C_RESET);
        return;
    }

    char config_path[1024];
    get_config_file_path(config_path, sizeof(config_path));

    FILE *in = fopen(config_path, "r");
    char temp_path[1024];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", config_path);
    FILE *out = fopen(temp_path, "w");

    if (!out) {
        printf("%s[-] Cannot write config file: %s%s\n", C_RED, temp_path, C_RESET);
        if (in) fclose(in);
        return;
    }

    int updated = 0;
    if (in) {
        char line[2048];
        while (fgets(line, sizeof(line), in)) {
            char current_tag[128] = {0};
            char current_path[1024] = {0};

            if (sscanf(line, "%127s = %1023[^\r\n]", current_tag, current_path) == 2) {
                if (strcmp(current_tag, tag) == 0) {
                    fprintf(out, "%s = %s\n", tag, abs_path);
                    updated = 1;
                    continue;
                }
            }
            fputs(line, out);
        }
        fclose(in);
    }

    if (!updated) {
        fprintf(out, "%s = %s\n", tag, abs_path);
    }

    fclose(out);

    remove(config_path);
    rename(temp_path, config_path);

    printf("%s[✔] Bookmarked tag '%s%s%s' -> %s%s%s\n",
           C_GREEN, C_BOLD, tag, C_GREEN, C_BOLD, abs_path, C_RESET);
}

/* --- COMMAND: warp <tag> --- */
static void cmd_warp(const char *tag) {
    char config_path[1024];
    get_config_file_path(config_path, sizeof(config_path));

    FILE *in = fopen(config_path, "r");
    if (!in) {
        printf("%s[-] No bookmarks found. Use 'mark <tag>' first!%s\n", C_RED, C_RESET);
        return;
    }

    char line[2048];
    int found = 0;
    while (fgets(line, sizeof(line), in)) {
        char current_tag[128] = {0};
        char current_path[1024] = {0};

        if (sscanf(line, "%127s = %1023[^\r\n]", current_tag, current_path) == 2) {
            if (strcmp(current_tag, tag) == 0) {
                printf("%s\n", current_path);
                found = 1;
                break;
            }
        }
    }
    fclose(in);

    if (!found) {
        fprintf(stderr, "%s[-] Bookmark tag '%s' not found!%s\n", C_RED, tag, C_RESET);
    }
}

/* --- COMMAND: unmark <tag> --- */
static void cmd_unmark(const char *tag) {
    char config_path[1024];
    get_config_file_path(config_path, sizeof(config_path));

    FILE *in = fopen(config_path, "r");
    if (!in) {
        printf("%s[-] No bookmarks file found.%s\n", C_RED, C_RESET);
        return;
    }

    char temp_path[1024];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", config_path);
    FILE *out = fopen(temp_path, "w");

    if (!out) {
        fclose(in);
        printf("%s[-] Error writing config file.%s\n", C_RED, C_RESET);
        return;
    }

    char line[2048];
    int removed = 0;
    while (fgets(line, sizeof(line), in)) {
        char current_tag[128] = {0};
        char current_path[1024] = {0};

        if (sscanf(line, "%127s = %1023[^\r\n]", current_tag, current_path) == 2) {
            if (strcmp(current_tag, tag) == 0) {
                removed = 1;
                continue;
            }
        }
        fputs(line, out);
    }

    fclose(in);
    fclose(out);

    remove(config_path);
    rename(temp_path, config_path);

    if (removed) {
        printf("%s[✔] Removed bookmark tag '%s%s%s'!%s\n", C_GREEN, C_BOLD, tag, C_GREEN, C_RESET);
    } else {
        printf("%s[-] Bookmark tag '%s' not found.%s\n", C_RED, tag, C_RESET);
    }
}

/* --- COMMAND: list --- */
static void cmd_list(void) {
    char config_path[1024];
    get_config_file_path(config_path, sizeof(config_path));

    FILE *in = fopen(config_path, "r");
    if (!in) {
        printf("%s[-] No bookmarks saved yet. Use 'mark <tag>' to add one!%s\n", C_YELLOW, C_RESET);
        return;
    }

    printf("%s[+] Active Bookmarks (~/.markwarp_bookmarks):%s\n", C_CYAN, C_RESET);
    char line[2048];
    int count = 0;

    while (fgets(line, sizeof(line), in)) {
        char current_tag[128] = {0};
        char current_path[1024] = {0};

        if (sscanf(line, "%127s = %1023[^\r\n]", current_tag, current_path) == 2) {
            printf("  ├─ %s%-16s%s : %s\n", C_BOLD, current_tag, C_RESET, current_path);
            count++;
        }
    }
    fclose(in);

    if (count == 0) {
        printf("  └─ %s[!] No bookmarks stored.%s\n", C_YELLOW, C_RESET);
    }
}

/* --- Help Menu --- */
static void print_help(const char *prog_name) {
    printf("%smarkwarp - Fast Directory Bookmarking CLI Tool v1.0%s\n", C_BOLD, C_RESET);
    printf("Usage: %s <command> [args]\n\n", prog_name);
    printf("Commands:\n");
    printf("  mark <tag> [path]    Bookmark a directory (defaults to current folder)\n");
    printf("  warp <tag>           Get the bookmarked directory path\n");
    printf("  unmark <tag>         Remove a bookmarked tag\n");
    printf("  list                 List all saved bookmarks\n");
    printf("  help                 Show this help menu\n\n");
}

/* --- Main Entry Point --- */
int main(int argc, char *argv[]) {
    init_tty_check();

    if (argc < 2) {
        print_help(argv[0]);
        return 0;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "mark") == 0 || strcmp(cmd, "-mark") == 0) {
        if (argc < 3) {
            printf("%s[-] Missing tag name. Usage: markwarp mark <tag> [path]%s\n", C_RED, C_RESET);
            return 1;
        }
        const char *path = (argc >= 4) ? argv[3] : ".";
        cmd_mark(argv[2], path);
    }
    else if (strcmp(cmd, "warp") == 0 || strcmp(cmd, "-warp") == 0) {
        if (argc < 3) {
            printf("%s[-] Missing tag name. Usage: markwarp warp <tag>%s\n", C_RED, C_RESET);
            return 1;
        }
        cmd_warp(argv[2]);
    }
    else if (strcmp(cmd, "unmark") == 0 || strcmp(cmd, "-unmark") == 0) {
        if (argc < 3) {
            printf("%s[-] Missing tag name. Usage: markwarp unmark <tag>%s\n", C_RED, C_RESET);
            return 1;
        }
        cmd_unmark(argv[2]);
    }
    else if (strcmp(cmd, "list") == 0 || strcmp(cmd, "-list") == 0 || strcmp(cmd, "marks") == 0) {
        cmd_list();
    }
    else {
        print_help(argv[0]);
    }

    return 0;
}
