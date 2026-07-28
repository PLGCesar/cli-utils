/*
 * note.c v2.0 - Smart CLI Note & Priority Reminder Manager
 * Features: Create, list, edit notes in Markdown and set priority reminders for shell sessions.
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

/* --- OS & Terminal Compatibility Layer --- */
#if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS 1
    #include <windows.h>
    #include <conio.h>
    #include <direct.h>
    #include <io.h>
    #define mkdir_native(path) _mkdir(path)
#else
    #define OS_UNIX 1
    #include <termios.h>
    #include <unistd.h>
    #define mkdir_native(path) mkdir(path, 0755)
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
#define C_DIM     (use_colors ? "\033[2m"  : "")

/* --- Directory Creation Helper (mkdir -p) --- */
static void create_dir_p(const char *path) {
    char temp[2048];
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

/* --- Get Notes Storage Directory (~/.notes) --- */
static void get_notes_dir(char *out_path, size_t max_len) {
    const char *home = NULL;
#ifdef OS_WINDOWS
    home = getenv("USERPROFILE");
    if (!home) home = getenv("HOMEDRIVE");
#else
    home = getenv("HOME");
#endif
    if (!home) home = ".";

    snprintf(out_path, max_len, "%s/.notes", home);
    create_dir_p(out_path);
}

/* --- Priority Helper --- */
static void get_note_priority(const char *dir, const char *name, char *out_priority, size_t max_len) {
    char meta_path[2048];
    snprintf(meta_path, sizeof(meta_path), "%s/%s.meta", dir, name);

    FILE *fp = fopen(meta_path, "r");
    if (fp) {
        if (fgets(out_priority, (int)max_len, fp)) {
            out_priority[strcspn(out_priority, "\r\n")] = '\0';
        } else {
            strncpy(out_priority, "low", max_len - 1);
        }
        fclose(fp);
    } else {
        strncpy(out_priority, "low", max_len - 1);
    }
}

static void set_note_priority(const char *dir, const char *name, const char *priority) {
    char meta_path[2048];
    snprintf(meta_path, sizeof(meta_path), "%s/%s.meta", dir, name);

    FILE *fp = fopen(meta_path, "w");
    if (fp) {
        fprintf(fp, "%s\n", priority);
        fclose(fp);
    }
}

/* --- Open Text Editor Helper --- */
static void open_editor(const char *file_path) {
    const char *editor = getenv("EDITOR");
    if (!editor || strlen(editor) == 0) {
#ifdef OS_WINDOWS
        editor = "notepad";
#else
        editor = "nano";
#endif
    }

    char command[2048];
    snprintf(command, sizeof(command), "%s \"%s\"", editor, file_path);
    int res = system(command);
    (void)res;
}

/* --- COMMAND: note add <name> [content] --- */
static void cmd_add(const char *name, const char *content) {
    char dir[1024];
    get_notes_dir(dir, sizeof(dir));

    char file_path[2048];
    snprintf(file_path, sizeof(file_path), "%s/%s.md", dir, name);

    if (content && strlen(content) > 0) {
        FILE *fp = fopen(file_path, "a");
        if (fp) {
            fprintf(fp, "%s\n", content);
            fclose(fp);
            printf("%s[✔] Appended text to note '%s%s%s'!%s\n", C_GREEN, C_BOLD, name, C_GREEN, C_RESET);
        } else {
            printf("%s[-] Failed to write note: %s%s\n", C_RED, file_path, C_RESET);
        }
    } else {
        printf("%s[+] Opening note '%s%s%s' in editor...%s\n", C_CYAN, C_BOLD, name, C_CYAN, C_RESET);
        open_editor(file_path);
    }
}

/* --- COMMAND: note show <name> --- */
static void cmd_show(const char *name) {
    char dir[1024];
    get_notes_dir(dir, sizeof(dir));

    char file_path[2048];
    snprintf(file_path, sizeof(file_path), "%s/%s.md", dir, name);

    FILE *fp = fopen(file_path, "r");
    if (!fp) {
        printf("%s[-] Note '%s' not found!%s\n", C_RED, name, C_RESET);
        return;
    }

    char priority[32];
    get_note_priority(dir, name, priority, sizeof(priority));

    printf("\n%s📝 Note: %s%s%s %s[%s]%s\n",
           C_CYAN, C_BOLD, name, C_RESET,
           (strcmp(priority, "high") == 0) ? C_RED : C_YELLOW, priority, C_RESET);
    printf("-----------------------------------------\n");

    char line[2048];
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }
    fclose(fp);
    printf("-----------------------------------------\n");
}

/* --- COMMAND: note priority <name> <high|medium|low> --- */
static void cmd_create_priority(const char *name, const char *priority) {
    char dir[1024];
    get_notes_dir(dir, sizeof(dir));

    char file_path[2048];
    snprintf(file_path, sizeof(file_path), "%s/%s.md", dir, name);

    struct stat st;
    if (stat(file_path, &st) != 0) {
        printf("%s[-] Note '%s' does not exist! Create it first with 'note add %s'%s\n",
               C_RED, name, name, C_RESET);
        return;
    }

    set_note_priority(dir, name, priority);
    printf("%s[✔] Set priority of note '%s%s%s' to %s%s%s!%s\n",
           C_GREEN, C_BOLD, name, C_GREEN, C_BOLD, priority, C_GREEN, C_RESET);
}

/* --- COMMAND: note list --- */
static void cmd_list(void) {
    char dir_path[1024];
    get_notes_dir(dir_path, sizeof(dir_path));

    DIR *dir = opendir(dir_path);
    if (!dir) return;

    printf("\n%s[+] Saved Notes (~/.notes/):%s\n", C_CYAN, C_RESET);
    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        char *ext = strrchr(entry->d_name, '.');
        if (ext && strcmp(ext, ".md") == 0) {
            char note_name[256];
            size_t name_len = (size_t)(ext - entry->d_name);
            if (name_len >= sizeof(note_name)) name_len = sizeof(note_name) - 1;
            strncpy(note_name, entry->d_name, name_len);
            note_name[name_len] = '\0';

            char priority[32];
            get_note_priority(dir_path, note_name, priority, sizeof(priority));

            const char *p_color = C_YELLOW;
            if (strcmp(priority, "high") == 0) p_color = C_RED;
            else if (strcmp(priority, "low") == 0) p_color = C_DIM;

            printf("  ├─ %s%-20s%s Priority: %s[%s]%s\n",
                   C_BOLD, note_name, C_RESET, p_color, priority, C_RESET);
            count++;
        }
    }
    closedir(dir);

    if (count == 0) {
        printf("  └─ %s[!] No notes saved yet. Create one with 'note add <name>'!%s\n", C_YELLOW, C_RESET);
    }
}

/* --- COMMAND: note remind (Shell Startup High-Priority Reminders) --- */
static void cmd_remind(void) {
    char dir_path[1024];
    get_notes_dir(dir_path, sizeof(dir_path));

    DIR *dir = opendir(dir_path);
    if (!dir) return;

    int high_count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        char *ext = strrchr(entry->d_name, '.');
        if (ext && strcmp(ext, ".md") == 0) {
            char note_name[256];
            size_t name_len = (size_t)(ext - entry->d_name);
            if (name_len >= sizeof(note_name)) name_len = sizeof(note_name) - 1;
            strncpy(note_name, entry->d_name, name_len);
            note_name[name_len] = '\0';

            char priority[32];
            get_note_priority(dir_path, note_name, priority, sizeof(priority));

            if (strcmp(priority, "high") == 0) {
                if (high_count == 0) {
                    printf("%s🔔 HIGH PRIORITY REMINDERS:%s\n", C_RED, C_RESET);
                }
                printf("  🚨 %s%s%s (Run 'note show %s' to read)\n",
                       C_BOLD, note_name, C_RESET, note_name);
                high_count++;
            }
        }
    }
    closedir(dir);
}

/* --- COMMAND: note rm <name> --- */
static void cmd_remove(const char *name) {
    char dir[1024];
    get_notes_dir(dir, sizeof(dir));

    char md_path[2048];
    char meta_path[2048];
    snprintf(md_path, sizeof(md_path), "%s/%s.md", dir, name);
    snprintf(meta_path, sizeof(meta_path), "%s/%s.meta", dir, name);

    remove(meta_path);
    if (remove(md_path) == 0) {
        printf("%s[✔] Deleted note '%s%s%s'!%s\n", C_GREEN, C_BOLD, name, C_GREEN, C_RESET);
    } else {
        printf("%s[-] Note '%s' not found.%s\n", C_RED, name, C_RESET);
    }
}

/* --- Help Menu --- */
static void print_help(const char *prog_name) {
    printf("%snote - Smart CLI Note & Reminder Manager v2.0%s\n", C_BOLD, C_RESET);
    printf("Usage: %s <command> [args]\n\n", prog_name);
    printf("Commands:\n");
    printf("  add <name> [text]         Create or append text to a note (opens editor if text omitted)\n");
    printf("  show <name>               Read note contents in terminal\n");
    printf("  list, ls                  List all saved notes and priorities\n");
    printf("  priority <name> <level>   Set priority level (high, medium, low)\n");
    printf("  remind                    Display high-priority reminders\n");
    printf("  rm <name>                 Delete a note\n");
    printf("  help                      Display this help menu\n\n");
}

/* --- Main Entry Point --- */
int main(int argc, char *argv[]) {
    init_tty_check();

    if (argc < 2) {
        print_help(argv[0]);
        return 0;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "add") == 0 || strcmp(cmd, "create") == 0) {
        if (argc < 3) { printf("%s[-] Missing note name. Usage: note add <name> [text]%s\n", C_RED, C_RESET); return 1; }
        const char *text = (argc >= 4) ? argv[3] : NULL;
        cmd_add(argv[2], text);
    }
    else if (strcmp(cmd, "show") == 0 || strcmp(cmd, "read") == 0) {
        if (argc < 3) { printf("%s[-] Missing note name. Usage: note show <name>%s\n", C_RED, C_RESET); return 1; }
        cmd_show(argv[2]);
    }
    else if (strcmp(cmd, "list") == 0 || strcmp(cmd, "ls") == 0) {
        cmd_list();
    }
    else if (strcmp(cmd, "priority") == 0 || strcmp(cmd, "prio") == 0) {
        if (argc < 4) { printf("%s[-] Missing args. Usage: note priority <name> <high|medium|low>%s\n", C_RED, C_RESET); return 1; }
        cmd_create_priority(argv[2], argv[3]);
    }
    else if (strcmp(cmd, "remind") == 0) {
        cmd_remind();
    }
    else if (strcmp(cmd, "rm") == 0 || strcmp(cmd, "delete") == 0) {
        if (argc < 3) { printf("%s[-] Missing note name. Usage: note rm <name>%s\n", C_RED, C_RESET); return 1; }
        cmd_remove(argv[2]);
    }
    else {
        print_help(argv[0]);
    }

    return 0;
}
