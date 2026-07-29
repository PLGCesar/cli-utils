/*
 * lit.c v1.3 - Ultra-lightweight Local Snapshot & Versioning CLI Utility
 * Adds: "diff" command and .litignore support
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
    #include <windows.h>
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

/* --- .litignore Support --- */
#define MAX_IGNORES 256
#define MAX_IGNORE_LEN 256
static char ignore_patterns[MAX_IGNORES][MAX_IGNORE_LEN];
static int ignore_count = 0;

static void load_litignore(void) {
    ignore_count = 0;
    FILE *f = fopen(".litignore", "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *s = line;
        while (*s && (*s == ' ' || *s == '\t')) s++;
        if (*s == '\0' || *s == '\n' || *s == '#') continue;
        // strip newline and trailing spaces
        char *e = s + strlen(s) - 1;
        while (e >= s && (*e == '\n' || *e == '\r' || *e == ' ' || *e == '\t')) { *e = '\0'; e--; }
        if (strlen(s) > 0 && ignore_count < MAX_IGNORES) {
            strncpy(ignore_patterns[ignore_count], s, MAX_IGNORE_LEN-1);
            ignore_patterns[ignore_count][MAX_IGNORE_LEN-1] = '\0';
            ignore_count++;
        }
    }
    fclose(f);
}

static int ends_with(const char *str, const char *suffix) {
    size_t l1 = strlen(str), l2 = strlen(suffix);
    if (l2 > l1) return 0;
    return strcmp(str + l1 - l2, suffix) == 0;
}

static int starts_with(const char *str, const char *prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

static int should_ignore(const char *relpath, int is_dir) {
    if (!relpath) return 0;
    // Always ignore .snapshot itself
    if (strcmp(relpath, ".snapshot") == 0) return 1;
    for (int i = 0; i < ignore_count; i++) {
        const char *p = ignore_patterns[i];
        size_t lp = strlen(p);
        if (lp == 0) continue;
        if (p[0] == '*') {
            // suffix match: *suffix
            if (ends_with(relpath, p + 1)) return 1;
        } else if (p[lp-1] == '*') {
            // prefix match: prefix*
            char tmp[512];
            strncpy(tmp, p, lp-1);
            tmp[lp-1] = '\0';
            if (starts_with(relpath, tmp)) return 1;
        } else if (p[lp-1] == '/') {
            // directory pattern
            if (is_dir && starts_with(relpath, p)) return 1;
            // also match basename when pattern equals dirname/
            if (strcmp(relpath, p) == 0) return 1;
        } else {
            // exact match or basename match
            if (strcmp(relpath, p) == 0) return 1;
            // also match if relpath ends with /<pattern>
            char tmp[512];
            snprintf(tmp, sizeof(tmp), "/%s", p);
            if (ends_with(relpath, tmp)) return 1;
        }
    }
    return 0;
}

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

    char buffer[65536];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        fwrite(buffer, 1, bytes, out);
    }

    fclose(in);
    fclose(out);
    return 1;
}

/* --- Recursively Wipe Working Directory (Ignores .snapshot and .litignore) --- */
static void wipe_working_dir(const char *path, const char *ignore_folder) {
    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        if (ignore_folder && strcmp(entry->d_name, ignore_folder) == 0)
            continue;

        char full_path[4096];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            int is_dir = S_ISDIR(st.st_mode);
            // build relative path from current directory
            char rel[4096];
            snprintf(rel, sizeof(rel), "%s", entry->d_name);
            if (should_ignore(rel, is_dir)) continue;

            if (is_dir) {
                wipe_working_dir(full_path, NULL);
                rmdir_native(full_path);
            } else {
                remove(full_path);
            }
        }
    }
    closedir(dir);
}

/* --- Recursive Directory Copy (Ignores .snapshot and .litignore) --- */
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

        char src_path[4096];
        char dst_path[4096];
        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, entry->d_name);

        struct stat st;
        if (stat(src_path, &st) == 0) {
            int is_dir = S_ISDIR(st.st_mode);
            // relative path for ignore checks (use name only here)
            char rel[4096];
            snprintf(rel, sizeof(rel), "%s", entry->d_name);
            if (should_ignore(rel, is_dir)) continue;

            if (is_dir) {
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

/* --- File content comparison --- */
static int files_differ(const char *a, const char *b) {
    FILE *fa = fopen(a, "rb");
    if (!fa) return 1;
    FILE *fb = fopen(b, "rb");
    if (!fb) { fclose(fa); return 1; }

    char bufA[65536], bufB[65536];
    size_t ra, rb;
    int diff = 0;
    while (1) {
        ra = fread(bufA, 1, sizeof(bufA), fa);
        rb = fread(bufB, 1, sizeof(bufB), fb);
        if (ra != rb || (ra > 0 && memcmp(bufA, bufB, ra) != 0)) { diff = 1; break; }
        if (ra == 0) break;
    }

    fclose(fa);
    fclose(fb);
    return diff;
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

    load_litignore();
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

    load_litignore();

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

    load_litignore();

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

    wipe_working_dir(".", ".snapshot");

    int files_copied = 0;
    copy_dir_recursive(snap_path, ".", NULL, &files_copied);

    printf("%s[✔] Restored snapshot '%s%s%s' (%d files restored)!%s\n",
           C_GREEN, C_BOLD, selected_snap, C_GREEN, files_copied, C_RESET);
}

/* --- COMMAND: lit -diff [name] --- */
static void cmd_diff(const char *snap_name) {
    struct stat st;
    if (stat(".snapshot", &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("%s[-] Repository not initialized! Run 'lit -init' first.%s\n", C_RED, C_RESET);
        return;
    }

    load_litignore();

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

    // Walk snapshot and compare to workspace
    int added = 0, removed = 0, modified = 0;

    // Helper: check snapshot -> workspace (deleted/modified)
    DIR *dir = opendir(snap_path);
    if (!dir) return;

    struct dirent *entry;
    // Use a stack-less recursion approach via helper function
    // We'll implement a small recursive lambda-like function using a nested function is not portable in C,
    // so create a static recursive function below and call it with root paths.

    // We'll call a recursive comparer implemented further down.
    // For simplicity reuse a quick inline recursive comparator here by declaring a function pointer not possible — implement functions below.
    // So call compare_dirs declared below.
    
    // We'll use compare_dirs which prints and updates counters.
    
    // Forward declare and call
    extern void compare_dirs(const char *snap_root, const char *work_root, const char *snap_rel, int *added, int *removed, int *modified);
    compare_dirs(snap_path, ".", "", &added, &removed, &modified);

    printf("%s[+] Diff against snapshot '%s%s%s':%s\n", C_CYAN, C_BOLD, selected_snap, C_CYAN, C_RESET);
    if (added == 0 && removed == 0 && modified == 0) {
        printf("  %s[=] No differences found.%s\n", C_GREEN, C_RESET);
    } else {
        if (added) printf("  %s+ %d added%s\n", C_GREEN, added, C_RESET);
        if (removed) printf("  %s- %d removed%s\n", C_RED, removed, C_RESET);
        if (modified) printf("  %s~ %d modified%s\n", C_YELLOW, modified, C_RESET);
    }
}

/* recursive comparer implementation */
static void compare_dirs_internal(const char *snap_root, const char *work_root, const char *snap_rel, int *added, int *removed, int *modified) {
    char snap_path[4096];
    char work_path[4096];
    snprintf(snap_path, sizeof(snap_path), "%s/%s", snap_root, snap_rel);
    if (strlen(snap_rel) == 0) snprintf(snap_path, sizeof(snap_path), "%s", snap_root);

    DIR *dir = opendir(snap_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char rel[4096];
        if (strlen(snap_rel) == 0) snprintf(rel, sizeof(rel), "%s", entry->d_name);
        else snprintf(rel, sizeof(rel), "%s/%s", snap_rel, entry->d_name);

        // ignore check
        struct stat st_snap;
        snprintf(snap_path, sizeof(snap_path), "%s/%s", snap_root, rel);
        if (stat(snap_path, &st_snap) != 0) continue;
        int is_dir = S_ISDIR(st_snap.st_mode);
        if (should_ignore(rel, is_dir)) continue;

        snprintf(work_path, sizeof(work_path), "%s/%s", work_root, rel);
        struct stat st_work;
        if (stat(work_path, &st_work) != 0) {
            // missing in workspace -> removed
            printf("  %s- %s%s\n", C_RED, rel, C_RESET);
            (*removed)++;
            continue;
        }
        if (is_dir) {
            compare_dirs_internal(snap_root, work_root, rel, added, removed, modified);
        } else {
            // compare content
            if (files_differ(snap_path, work_path)) {
                printf("  %s~ %s%s\n", C_YELLOW, rel, C_RESET);
                (*modified)++;
            }
        }
    }
    closedir(dir);

    // Now check workspace for added files under this rel
    char work_dir_path[4096];
    if (strlen(snap_rel) == 0) snprintf(work_dir_path, sizeof(work_dir_path), "%s", work_root);
    else snprintf(work_dir_path, sizeof(work_dir_path), "%s/%s", work_root, snap_rel);

    DIR *wdir = opendir(work_dir_path);
    if (!wdir) return;
    while ((entry = readdir(wdir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char rel2[4096];
        if (strlen(snap_rel) == 0) snprintf(rel2, sizeof(rel2), "%s", entry->d_name);
        else snprintf(rel2, sizeof(rel2), "%s/%s", snap_rel, entry->d_name);

        struct stat stw;
        char wfull[4096];
        snprintf(wfull, sizeof(wfull), "%s/%s", work_root, rel2);
        if (stat(wfull, &stw) != 0) continue;
        int is_dir_w = S_ISDIR(stw.st_mode);
        if (should_ignore(rel2, is_dir_w)) continue;

        // check if exists in snapshot
        char sfull[4096];
        snprintf(sfull, sizeof(sfull), "%s/%s", snap_root, rel2);
        if (stat(sfull, &stw) != 0) {
            printf("  %s+ %s%s\n", C_GREEN, rel2, C_RESET);
            (*added)++;
        }
    }
    closedir(wdir);
}

/* wrapper to satisfy forward declaration */
void compare_dirs(const char *snap_root, const char *work_root, const char *snap_rel, int *added, int *removed, int *modified) {
    compare_dirs_internal(snap_root, work_root, snap_rel, added, removed, modified);
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

    printf("%s[+] Saved Snapshots in .snapshot/: %s\n", C_CYAN, C_RESET);
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
    printf("%slit - Ultra-lightweight Local Versioning CLI Tool v1.3%s\n", C_BOLD, C_RESET);
    printf("Usage: %s <command> [args]\n\n", prog_name);
    printf("Commands:\n");
    printf("  -init, init              Initialize .snapshot/ and save initial v0/v1 snapshot\n");
    printf("  -snap, snap <name>       Save a new snapshot of current folder into .snapshot/<name>\n");
    printf("  -load, load [name|vX]    Clean workspace & restore specified or latest snapshot\n");
    printf("  -list, list              List all snapshots stored in .snapshot/\n");
    printf("  -diff, diff [name|vX]    Show differences between workspace and a snapshot (defaults to latest)\n");
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
    else if (strcmp(cmd, "-diff") == 0 || strcmp(cmd, "diff") == 0) {
        const char *snap = (argc >= 3) ? argv[2] : NULL;
        cmd_diff(snap);
    }
    else if (strcmp(cmd, "-list") == 0 || strcmp(cmd, "list") == 0) {
        cmd_list();
    }
    else {
        print_help(argv[0]);
    }

    return 0;
}
