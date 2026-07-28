/*
 * template.c v2.0 - Directory Structure Scaffolder & Template Engine
 * Features: Make, Load, Show (Tree Preview), Remove, Export to Downloads, Import.
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

/* --- Get User Downloads Directory --- */
static void get_downloads_dir(char *out_path, size_t size) {
    const char *home = NULL;
#ifdef OS_WINDOWS
    home = getenv("USERPROFILE");
    if (!home) home = getenv("HOMEDRIVE");
#else
    home = getenv("HOME");
#endif
    if (!home) home = ".";

    snprintf(out_path, size, "%s/Downloads", home);
    struct stat st;
    // Fallback to HOME if Downloads directory doesn't exist
    if (stat(out_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        snprintf(out_path, size, "%s", home);
    }
}

/* --- Binary File Copy Helper --- */
static int copy_file(const char *src_path, const char *dst_path) {
    FILE *src = fopen(src_path, "rb");
    if (!src) return 0;

    FILE *dst = fopen(dst_path, "wb");
    if (!dst) {
        fclose(src);
        return 0;
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }

    fclose(src);
    fclose(dst);
    return 1;
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
            // IGNORE FILES! Only record directories
            if (S_ISDIR(st.st_mode)) {
                fprintf(out_fp, "%s\n", new_rel_path);
                (*count)++;
                scan_dirs_recursive(full_path, new_rel_path, out_fp, count);
            }
        }
    }
    closedir(dir);
    return 1;
}

/* --- COMMAND: -make <name> --- */
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

    printf("%s[+] Sniffing current directory hierarchy...%s\n", C_CYAN, C_RESET);
    int count = 0;
    scan_dirs_recursive(".", "", fp, &count);
    fclose(fp);

    printf("%s[✔] Successfully saved template '%s%s%s' (%d folders mapped)!%s\n",
           C_GREEN, C_BOLD, tmpl_name, C_GREEN, count, C_RESET);
    printf("    Saved location: %s\n", tmpl_file);
}

/* --- COMMAND: -load <name> [target_dir] --- */
static void cmd_load_template(const char *tmpl_name, const char *target_dir) {
    char templates_dir[1024];
    get_templates_dir(templates_dir, sizeof(templates_dir));

    char tmpl_file[1024];
    snprintf(tmpl_file, sizeof(tmpl_file), "%s/%s.tmpl", templates_dir, tmpl_name);

    FILE *fp = fopen(tmpl_file, "r");
    if (!fp) {
        printf("%s[-] Template '%s' not found in ~/.templates/%s\n", C_RED, tmpl_name, C_RESET);
        printf("    Run 'template -list' to see saved templates.\n");
        return;
    }

    if (target_dir && strlen(target_dir) > 0) {
        create_dir_p(target_dir);
        printf("%s[+] Creating root project folder: %s/%s\n", C_CYAN, target_dir, C_RESET);
    }

    printf("%s[+] Unpacking template '%s%s%s'...%s\n",
           C_CYAN, C_BOLD, tmpl_name, C_CYAN, C_RESET);

    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) > 0) {
            char final_path[2048];
            if (target_dir && strlen(target_dir) > 0) {
                snprintf(final_path, sizeof(final_path), "%s/%s", target_dir, line);
            } else {
                snprintf(final_path, sizeof(final_path), "%s", line);
            }

            create_dir_p(final_path);
            printf("  ├─ %sCreated:%s %s/\n", C_GREEN, C_RESET, final_path);
            count++;
        }
    }
    fclose(fp);

    printf("%s[✔] Done! Unpacked %d folders successfully.%s\n", C_GREEN, count, C_RESET);
}

/* --- COMMAND: -show <name> (ASCII Tree Preview) --- */
static void cmd_show_template(const char *tmpl_name) {
    char templates_dir[1024];
    get_templates_dir(templates_dir, sizeof(templates_dir));

    char tmpl_file[1024];
    snprintf(tmpl_file, sizeof(tmpl_file), "%s/%s.tmpl", templates_dir, tmpl_name);

    FILE *fp = fopen(tmpl_file, "r");
    if (!fp) {
        printf("%s[-] Template '%s' not found!%s\n", C_RED, tmpl_name, C_RESET);
        return;
    }

    printf("\n%s[+] Tree structure preview for template '%s%s%s':%s\n",
           C_CYAN, C_BOLD, tmpl_name, C_CYAN, C_RESET);

    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) == 0) continue;

        // Calculate depth level by counting slashes
        int depth = 0;
        for (size_t i = 0; i < strlen(line); i++) {
            if (line[i] == '/' || line[i] == '\\') depth++;
        }

        // Extract last directory name in path
        char *last_slash = strrchr(line, '/');
        if (!last_slash) last_slash = strrchr(line, '\\');
        const char *folder_name = last_slash ? last_slash + 1 : line;

        // Print tree branch indentation
        printf("  ");
        for (int i = 0; i < depth; i++) {
            printf("│  ");
        }
        printf("├─ %s%s/%s\n", C_BOLD, folder_name, C_RESET);
        count++;
    }
    fclose(fp);

    if (count == 0) {
        printf("  └─ [!] Template is empty.\n");
    } else {
        printf("\n  └─ %sTotal: %d folders%s\n", C_YELLOW, count, C_RESET);
    }
}

/* --- COMMAND: -rm <name> (Delete Template) --- */
static void cmd_remove_template(const char *tmpl_name) {
    char templates_dir[1024];
    get_templates_dir(templates_dir, sizeof(templates_dir));

    char tmpl_file[1024];
    snprintf(tmpl_file, sizeof(tmpl_file), "%s/%s.tmpl", templates_dir, tmpl_name);

    if (remove(tmpl_file) == 0) {
        printf("%s[✔] Nuked template '%s%s%s' from storage!%s\n", C_GREEN, C_BOLD, tmpl_name, C_GREEN, C_RESET);
    } else {
        printf("%s[-] Could not delete '%s'. Template does not exist.%s\n", C_RED, tmpl_name, C_RESET);
    }
}

/* --- COMMAND: -export <name> (Yeet to Downloads) --- */
static void cmd_export_template(const char *tmpl_name) {
    char templates_dir[1024];
    get_templates_dir(templates_dir, sizeof(templates_dir));

    char tmpl_file[1024];
    snprintf(tmpl_file, sizeof(tmpl_file), "%s/%s.tmpl", templates_dir, tmpl_name);

    // Check if template exists
    struct stat st;
    if (stat(tmpl_file, &st) != 0) {
        printf("%s[-] Template '%s' not found!%s\n", C_RED, tmpl_name, C_RESET);
        return;
    }

    char downloads_dir[1024];
    get_downloads_dir(downloads_dir, sizeof(downloads_dir));

    char dest_file[1024];
    snprintf(dest_file, sizeof(dest_file), "%s/%s.tmpl", downloads_dir, tmpl_name);

    if (copy_file(tmpl_file, dest_file)) {
        printf("%s[✔] Successfully exported template '%s%s%s' to Downloads folder!%s\n",
               C_GREEN, C_BOLD, tmpl_name, C_GREEN, C_RESET);
        printf("    File location: %s\n", dest_file);
    } else {
        printf("%s[-] Failed to copy file to Downloads folder.%s\n", C_RED, C_RESET);
    }
}

/* --- COMMAND: -import <file_path> --- */
static void cmd_import_template(const char *file_path) {
    struct stat st;
    if (stat(file_path, &st) != 0) {
        printf("%s[-] File not found: %s%s\n", C_RED, file_path, C_RESET);
        return;
    }

    // Extract filename from path
    const char *last_slash = strrchr(file_path, '/');
    if (!last_slash) last_slash = strrchr(file_path, '\\');
    const char *filename = last_slash ? last_slash + 1 : file_path;

    char templates_dir[1024];
    get_templates_dir(templates_dir, sizeof(templates_dir));

    char dest_file[1024];
    snprintf(dest_file, sizeof(dest_file), "%s/%s", templates_dir, filename);

    if (copy_file(file_path, dest_file)) {
        printf("%s[✔] Successfully imported template '%s%s%s' into ~/.templates/!%s\n",
               C_GREEN, C_BOLD, filename, C_GREEN, C_RESET);
    } else {
        printf("%s[-] Failed to import template file.%s\n", C_RED, C_RESET);
    }
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
            *ext = '\0';
            printf("  ├─ %s%s%s\n", C_BOLD, entry->d_name, C_RESET);
            count++;
        }
    }
    closedir(dir);

    if (count == 0) {
        printf("  └─ %s[x] No templates saved yet.%s\n", C_YELLOW, C_RESET);
    }
}

/* --- Help Menu --- */
static void print_help(const char *prog_name) {
    printf("%sTemplate Directory Scaffolder CLI v2.0%s\n", C_BOLD, C_RESET);
    printf("Usage: %s <command> [args]\n\n", prog_name);
    printf("Commands:\n");
    printf("  -make, make <name>          Map current directory hierarchy -> ~/.templates/<name>.tmpl\n");
    printf("  -load, load <name> [folder] Create directory structure (optionally in a target folder)\n");
    printf("  -show, show <name>          Display visual ASCII tree preview of template\n");
    printf("  -rm, rm, delete <name>      Delete saved template from ~/.templates/\n");
    printf("  -export, export <name>      Copy template file directly to Downloads folder\n");
    printf("  -import, import <file>      Import a .tmpl file into ~/.templates/\n");
    printf("  -list, list                 List all saved templates\n");
    printf("  -help, help                 Show this help menu\n\n");
}

/* --- Main Entry Point --- */
int main(int argc, char *argv[]) {
    init_tty_check();

    if (argc < 2) {
        print_help(argv[0]);
        return 0;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "-make") == 0 || strcmp(cmd, "make") == 0) {
        if (argc < 3) { printf("%s[-] Missing name. Usage: template -make <name>%s\n", C_RED, C_RESET); return 1; }
        cmd_make_template(argv[2]);
    }
    else if (strcmp(cmd, "-load") == 0 || strcmp(cmd, "load") == 0) {
        if (argc < 3) { printf("%s[-] Missing name. Usage: template -load <name> [target_folder]%s\n", C_RED, C_RESET); return 1; }
        const char *target = (argc >= 4) ? argv[3] : NULL;
        cmd_load_template(argv[2], target);
    }
    else if (strcmp(cmd, "-show") == 0 || strcmp(cmd, "show") == 0) {
        if (argc < 3) { printf("%s[-] Missing name. Usage: template -show <name>%s\n", C_RED, C_RESET); return 1; }
        cmd_show_template(argv[2]);
    }
    else if (strcmp(cmd, "-rm") == 0 || strcmp(cmd, "rm") == 0 || strcmp(cmd, "delete") == 0 || strcmp(cmd, "-delete") == 0) {
        if (argc < 3) { printf("%s[-] Missing name. Usage: template -rm <name>%s\n", C_RED, C_RESET); return 1; }
        cmd_remove_template(argv[2]);
    }
    else if (strcmp(cmd, "-export") == 0 || strcmp(cmd, "export") == 0) {
        if (argc < 3) { printf("%s[-] Missing name. Usage: template -export <name>%s\n", C_RED, C_RESET); return 1; }
        cmd_export_template(argv[2]);
    }
    else if (strcmp(cmd, "-import") == 0 || strcmp(cmd, "import") == 0) {
        if (argc < 3) { printf("%s[-] Missing file path. Usage: template -import <file.tmpl>%s\n", C_RED, C_RESET); return 1; }
        cmd_import_template(argv[2]);
    }
    else if (strcmp(cmd, "-list") == 0 || strcmp(cmd, "list") == 0) {
        cmd_list_templates();
    }
    else {
        print_help(argv[0]);
    }

    return 0;
}
