/*
 * template.c - Directory Structure Scaffolder & Template Engine
 * Captures current directory hierarchy and reproduces it anywhere!
 * Supports: Linux, macOS, Windows, Android (Termux).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS 1
    #include <direct.h>
    #include <io.h>
    #define mkdir_native(path) _mkdir(path)
#else
    #define OS_UNIX 1
    #include <unistd.h>
    #define mkdir_native(path) mkdir(path, 0755)
#endif

/* --- TTY & Colors --- */
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

/* --- Directory Creation Helper (mkdir -p equivalent) --- */
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

/* --- Get User Templates Directory (~/.templates) --- */
static void get_templates_dir(char *out_path, size_t size) {
    const char *home = NULL;
#ifdef OS_WINDOWS
    home = getenv("USERPROFILE");
    if (!home) home = getenv("HOMEDRIVE");
#else
    home = getenv("HOME");
#endif
    if (!home) home = ".";

    snprintf(out_path, size, "%s/.templates", home);
    create_dir_p(out_path);
}

/* --- Recursive Directory Scanner (Ignores Files) --- */
static int scan_dirs_recursive(const char *base_path, const char *rel_path, FILE *out_fp, int *count) {
    DIR *dir = opendir(base_path);
    if (!dir) return 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Skip hidden directories (like .git, .vscode, .templates)
        if (entry->d_name[0] == '.')
            continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

        char new_rel_path[1024];
        if (strlen(rel_path) == 0) {
            snprintf(new_rel_path, sizeof(new_rel_path), "%s", entry->d_name);
        } else {
            snprintf(new_rel_path, sizeof(new_rel_path), "%s/%s", rel_path, entry->d_name);
        }

        struct stat st;
        if (stat(full_path, &st) == 0) {
            // IGNORE FILES! Check if it's a directory
            if (S_ISDIR(st.st_mode)) {
                fprintf(out_fp, "%s\n", new_rel_path);
                (*count)++;
                // Recurse down subdirectories
                scan_dirs_recursive(full_path, new_rel_path, out_fp, count);
            }
        }
    }
    closedir(dir);
    return 1;
}

/* --- COMMAND: -make [name] --- */
static void cmd_make_template(const char *tmpl_name) {
    char templates_dir[1024];
    get_templates_dir(templates_dir, sizeof(templates_dir));

    char tmpl_file[1024];
    snprintf(tmpl_file, sizeof(tmpl_file), "%s/%s.tmpl", templates_dir, tmpl_name);

    FILE *fp = fopen(tmpl_file, "w");
    if (!fp) {
        printf("%s[-] Failed to write template file: %s%s\n", C_RED, tmpl_file, C_RESET);
        return;
    }

    printf("%s[+] Scanning current directory hierarchy...%s\n", C_CYAN, C_RESET);
    int count = 0;
    scan_dirs_recursive(".", "", fp, &count);
    fclose(fp);

    printf("%s[✔] Successfully saved template '%s%s%s' with %d directory structures!%s\n",
           C_GREEN, C_BOLD, tmpl_name, C_GREEN, count, C_RESET);
    printf("    Saved in: %s\n", tmpl_file);
}

/* --- COMMAND: -load [name] --- */
static void cmd_load_template(const char *tmpl_name) {
    char templates_dir[1024];
    get_templates_dir(templates_dir, sizeof(templates_dir));

    char tmpl_file[1024];
    snprintf(tmpl_file, sizeof(tmpl_file), "%s/%s.tmpl", templates_dir, tmpl_name);

    FILE *fp = fopen(tmpl_file, "r");
    if (!fp) {
        printf("%s[-] Template '%s' not found in ~/.templates/%s\n", C_RED, tmpl_name, C_RESET);
        printf("    Run 'template -list' to see all available templates.\n");
        return;
    }

    printf("%s[+] Unpacking template '%s%s%s' in current directory...%s\n",
           C_CYAN, C_BOLD, tmpl_name, C_CYAN, C_RESET);

    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
        // Strip trailing newline
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) > 0) {
            create_dir_p(line);
            printf("  ├─ %sCreated:%s %s/\n", C_GREEN, C_RESET, line);
            count++;
        }
    }
    fclose(fp);

    printf("%s[✔] Done! Created %d folders successfully.%s\n", C_GREEN, count, C_RESET);
}

/* --- COMMAND: -list --- */
static void cmd_list_templates(void) {
    char templates_dir[1024];
    get_templates_dir(templates_dir, sizeof(templates_dir));

    printf("%s[+] Saved Templates in ~/.templates/:%s\n", C_CYAN, C_RESET);

    DIR *dir = opendir(templates_dir);
    if (!dir) {
        printf("  └─ %s[!] No templates directory found.%s\n", C_YELLOW, C_RESET);
        return;
    }

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        char *ext = strrchr(entry->d_name, '.');
        if (ext && strcmp(ext, ".tmpl") == 0) {
            *ext = '\0'; // Trim extension
            printf("  ├─ %s%s%s\n", C_BOLD, entry->d_name, C_RESET);
            count++;
        }
    }
    closedir(dir);

    if (count == 0) {
        printf("  └─ %s[x] No templates saved yet. Create one with 'template -make <name>'!%s\n", C_YELLOW, C_RESET);
    }
}

/* --- Help Screen --- */
static void print_help(const char *prog_name) {
    printf("%sTemplate Directory Scaffolder CLI v1.0%s\n", C_BOLD, C_RESET);
    printf("Usage: %s <command> [template_name]\n\n", prog_name);
    printf("Commands:\n");
    printf("  -make, make <name>   Map current directory structure and save to ~/.templates/<name>.tmpl\n");
    printf("  -load, load <name>   Create directory hierarchy from template in current path\n");
    printf("  -list, list          List all saved templates in ~/.templates/\n");
    printf("  -help, help          Show this help menu\n\n");
}

/* --- Main Entry Point --- */
int main(int argc, char *argv[]) {
    init_tty_check();

    if (argc < 2) {
        print_help(argv[0]);
        return 0;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "-make") == 0 || strcmp(cmd, "--make") == 0 || strcmp(cmd, "make") == 0) {
        if (argc < 3) {
            printf("%s[-] Missing template name. Usage: template -make <name>%s\n", C_RED, C_RESET);
            return 1;
        }
        cmd_make_template(argv[2]);
    }
    else if (strcmp(cmd, "-load") == 0 || strcmp(cmd, "--load") == 0 || strcmp(cmd, "load") == 0) {
        if (argc < 3) {
            printf("%s[-] Missing template name. Usage: template -load <name>%s\n", C_RED, C_RESET);
            return 1;
        }
        cmd_load_template(argv[2]);
    }
    else if (strcmp(cmd, "-list") == 0 || strcmp(cmd, "--list") == 0 || strcmp(cmd, "list") == 0) {
        cmd_list_templates();
    }
    else if (strcmp(cmd, "-help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "help") == 0) {
        print_help(argv[0]);
    }
    else {
        printf("%s[-] Unknown command '%s'.%s\n", C_RED, cmd, C_RESET);
        print_help(argv[0]);
        return 1;
    }

    return 0;
}
