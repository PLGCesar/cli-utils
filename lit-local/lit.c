/*
 * lit.c v1.1 - Ultra-lightweight Local Snapshot & Versioning CLI Utility
 * A simple offline git alternative that snapshots files/folders into .snapshot/
 * Fix: Clean workspace before loading snapshots to ensure exact 1:1 state restore!
 * Supports: Linux, macOS, Windows, Android (Termux).
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS 1
    #include <direct.h>
    #include <io.h>
    #define mkdir_native(path) _mkdir(path)
    #define rmdir_native(path) _rmdir(path)
#else
    #define OS_UNIX 1
    #include <unistd.h>
    #define mkdir_native(path) mkdir(path, 0755)
    #define rmdir_native(path) rmdir(path)
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

/* --- Directory Creation Helper (mkdir -p) --- */
static void create_dir_p(const char *path) {
    char temp[1024];
    snprintf(temp, sizeof(temp), "%s", path);
    size_t len = strlen(temp);
    if (len == 0) return;

    for (size_t i = 0; i < len; i++) {
        if (temp[i] == '/' || temp[i] == '\\') {
            temp[i] = '\0';
            if (strlen(temp) > 0) {
                mkdir_native(temp);
            }
            temp[i] = '/';
        }
    }
    mkdir_native(temp);
}

/* --- Binary File Copy Helper --- */
static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return 0;

    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return 0;
    }

    char buffer[65536]; // 64KB chunk
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        fwrite(buffer, 1, bytes, out);
    }

    fclose(in);
    fclose(out);
    return 1;
}

/* --- Recursively Wipe Working Directory (Ignores .snapshot) --- */
static void wipe_working_dir(const char *path, const char *ignore_folder) {
    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // DO NOT DELETE .snapshot folder!
        if (ignore_folder && strcmp(entry->d_name, ignore_folder) == 0)
            continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                wipe_working_dir(full_path, NULL);
                rmdir_native(full_path);
            } else {
                remove(full_path);
            }
        }
    }
    closedir(dir);
}

/* --- Recursive Directory Copy (Ignores .snapshot) --- */
static void copy_dir_recursive(const char *src_dir, const char *dst_dir, const char *ignore_folder, int *files_copied) {
    DIR *dir = opendir(src_dir);
    if (!dir) return;

    create_dir_p(dst_dir);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        if (ignore_folder && strcmp(entry->d_name, ignore_folder) == 0)
            continue;

        char src_path[1024];
        char dst_path[1024];
        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, entry->d_name);

        struct stat st;
        if (stat(src_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                copy_dir_recursive(src_path, dst_path, ignore_folder, files_copied);
            } else if (S_ISREG(st.st_mode)) {
                if (copy_file(src_path, dst_path)) {
                    if (files_copied) (*files_copied)++;
                }
            }
        }
    }
    closedir(dir);
}

/* --- Helper: Get Next Auto Version Number (v0, v1, v2...) --- */
static int get_next_version_num(void) {
    DIR *dir = opendir(".snapshot");
    if (!dir) return 0;

    int max_v = -1;
    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char path[1024];
        snprintf(path, sizeof(path), ".snapshot/%s", entry->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            count++;
            if (entry->d_name[0] == 'v') {
                int v = atoi(entry->d_name + 1);
                if (v > max_v) max_v = v;
            }
        }
    }
    closedir(dir);

    if (max_v >= 0) return max_v + 1;
    return count;
}

/* --- Helper: Get Latest Modified Snapshot Name --- */
static int get_latest_snapshot_name(char *out_name, size_t size) {
    out_name[0] = '\0';
    DIR *dir = opendir(".snapshot");
    if (!dir) return 0;

    time_t max_time = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char path[1024];
        snprintf(path, sizeof(path), ".snapshot/%s", entry->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (st.st_mtime >= max_time) {
                max_time = st.st_mtime;
                snprintf(out_name, size, "%s", entry->d_name);
            }
        }
    }
    closedir(dir);
    return (strlen(out_name) > 0);
}

/* --- COMMAND: lit -init --- */
static void cmd_init(void) {
    struct stat st;
    int snapshot_existed = (stat(".snapshot", &st) == 0 && S_ISDIR(st.st_mode));

    create_dir_p(".snapshot");

    int ver_num = get_next_version_num();
    char version_name[32];
    snprintf(version_name, sizeof(version_name), "v%d", ver_num);

    char target_path[1024];
    snprintf(target_path, sizeof(target_path), ".snapshot/%s", version_name);

    int files_copied = 0;
    printf("%s[+] Initializing local lit repository in .snapshot/%s\n", C_CYAN, C_RESET);
    copy_dir_recursive(".", target_path, ".snapshot", &files_copied);

    if (!snapshot_existed && ver_num == 0) {
        printf("%s[✔] Initialized empty .snapshot repository and created initial snapshot '%s%s%s' (%d files)!%s\n",
               C_GREEN, C_BOLD, version_name, C_GREEN, files_copied, C_RESET);
    } else {
        printf("%s[✔] Initialized .snapshot repository and created snapshot '%s%s%s' (%d files)!%s\n",
               C_GREEN, C_BOLD, version_name, C_GREEN, files_copied, C_RESET);
    }
}

/* --- COMMAND: lit -snap <name> --- */
static void cmd_snap(const char *snap_name) {
    struct stat st;
    if (stat(".snapshot", &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("%s[-] Repository not initialized! Run 'lit -init' first.%s\n", C_RED, C_RESET);
        return;
    }

    char target_path[1024];
    snprintf(target_path, sizeof(target_path), ".snapshot/%s", snap_name);

    printf("%s[+] Taking snapshot '%s%s%s'...%s\n", C_CYAN, C_BOLD, snap_name, C_CYAN, C_RESET);
    int files_copied = 0;
    copy_dir_recursive(".", target_path, ".snapshot", &files_copied);

    printf("%s[✔] Saved snapshot '%s%s%s' (%d files recorded)!%s\n",
           C_GREEN, C_BOLD, snap_name, C_GREEN, files_copied, C_RESET);
}

/* --- COMMAND: lit -load [name] --- */
static void cmd_load(const char *snap_name) {
    struct stat st;
    if (stat(".snapshot", &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("%s[-] Repository not initialized! Run 'lit -init' first.%s\n", C_RED, C_RESET);
        return;
    }

    char selected_snap[128];
    if (snap_name && strlen(snap_name) > 0) {
        snprintf(selected_snap, sizeof(selected_snap), "%s", snap_name);
    } else {
        if (!get_latest_snapshot_name(selected_snap, sizeof(selected_snap))) {
            printf("%s[-] No snapshots found in .snapshot/%s\n", C_RED, C_RESET);
            return;
        }
        printf("%s[i] No version specified. Defaulting to latest snapshot '%s%s%s'.%s\n",
               C_YELLOW, C_BOLD, selected_snap, C_YELLOW, C_RESET);
    }

    char snap_path[1024];
    snprintf(snap_path, sizeof(snap_path), ".snapshot/%s", selected_snap);

    if (stat(snap_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("%s[-] Snapshot '%s' does not exist in .snapshot/%s\n", C_RED, selected_snap, C_RESET);
        return;
    }

    printf("%s[+] Cleaning current workspace and restoring snapshot '%s%s%s'...%s\n", C_CYAN, C_BOLD, selected_snap, C_CYAN, C_RESET);

    // 1. Wipe current workspace completely (protecting .snapshot folder)
    wipe_working_dir(".", ".snapshot");

    // 2. Unpack snapshot back into workspace
    int files_copied = 0;
    copy_dir_recursive(snap_path, ".", NULL, &files_copied);

    printf("%s[✔] Restored snapshot '%s%s%s' (%d files restored)!%s\n",
           C_GREEN, C_BOLD, selected_snap, C_GREEN, files_copied, C_RESET);
}

/* --- COMMAND: lit -list --- */
static void cmd_list(void) {
    struct stat st;
    if (stat(".snapshot", &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("%s[-] Repository not initialized! Run 'lit -init' first.%s\n", C_RED, C_RESET);
        return;
    }

    DIR *dir = opendir(".snapshot");
    if (!dir) return;

    printf("%s[+] Saved Snapshots in .snapshot/:%s\n", C_CYAN, C_RESET);
    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char path[1024];
        snprintf(path, sizeof(path), ".snapshot/%s", entry->d_name);
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            printf("  ├─ %s%s%s\n", C_BOLD, entry->d_name, C_RESET);
            count++;
        }
    }
    closedir(dir);

    if (count == 0) {
        printf("  └─ %s[!] No snapshots recorded yet.%s\n", C_YELLOW, C_RESET);
    }
}

/* --- Help Menu --- */
static void print_help(const char *prog_name) {
    printf("%slit - Ultra-lightweight Local Versioning CLI Tool v1.1%s\n", C_BOLD, C_RESET);
    printf("Usage: %s <command> [args]\n\n", prog_name);
    printf("Commands:\n");
    printf("  -init, init              Initialize .snapshot/ and save initial v0/v1 snapshot\n");
    printf("  -snap, snap <name>       Save a new snapshot of current folder into .snapshot/<name>\n");
    printf("  -load, load [name|vX]    Clean workspace & restore specified or latest snapshot\n");
    printf("  -list, list              List all snapshots stored in .snapshot/\n");
    printf("  -help, help              Display this help menu\n\n");
}

/* --- Main Entry Point --- */
int main(int argc, char *argv[]) {
    init_tty_check();

    if (argc < 2) {
        print_help(argv[0]);
        return 0;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "-init") == 0 || strcmp(cmd, "init") == 0) {
        cmd_init();
    }
    else if (strcmp(cmd, "-snap") == 0 || strcmp(cmd, "snap") == 0) {
        if (argc < 3) {
            printf("%s[-] Missing snapshot name. Usage: lit -snap <name>%s\n", C_RED, C_RESET);
            return 1;
        }
        cmd_snap(argv[2]);
    }
    else if (strcmp(cmd, "-load") == 0 || strcmp(cmd, "load") == 0) {
        const char *snap = (argc >= 3) ? argv[2] : NULL;
        cmd_load(snap);
    }
    else if (strcmp(cmd, "-list") == 0 || strcmp(cmd, "list") == 0) {
        cmd_list();
    }
    else {
        print_help(argv[0]);
    }

    return 0;
}
