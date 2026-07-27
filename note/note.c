/*
 * note.c - Terminal-based centered markdown note editor and manager.
 * Zero external heavy dependencies. Built for POSIX terminals.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <ctype.h>

#define MAX_BUF 16384
#define PATH_MAX_LEN 1024

static struct termios orig_termios;
static int raw_mode_active = 0;
static char current_note_name[256] = {0};
static char text_buf[MAX_BUF] = {0};
static int buf_len = 0;
static int cursor_pos = 0;

// Get full directory path (~/.notes)
static void get_notes_dir(char *buf, size_t size) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buf, size, "%s/.notes", home);
}

// Ensure ~/.notes directory exists
static void ensure_notes_dir(void) {
    char dir[PATH_MAX_LEN];
    get_notes_dir(dir, sizeof(dir));
#if defined(_WIN32)
    mkdir(dir);
#else
    mkdir(dir, 0755);
#endif
}

// Restore normal terminal screen and mode on exit
static void disable_raw_mode(void) {
    if (raw_mode_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_active = 0;
    }
    printf("\033[?25h\033[0m\033[2J\033[H"); // Show cursor, reset color, clear screen
    fflush(stdout);
}

// Enable raw mode char-by-char terminal input
static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    raw_mode_active = 1;

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN); // Keep ISIG to capture Ctrl+C
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Get priority for a note (stored in ~/.notes/<name>.meta)
static void get_note_priority(const char *name, char *prio_buf, size_t size) {
    char dir[PATH_MAX_LEN], meta_path[PATH_MAX_LEN];
    get_notes_dir(dir, sizeof(dir));
    snprintf(meta_path, sizeof(meta_path), "%s/%s.meta", dir, name);

    FILE *f = fopen(meta_path, "r");
    if (f) {
        if (fgets(prio_buf, size, f)) {
            prio_buf[strcspn(prio_buf, "\r\n")] = 0;
        } else {
            strncpy(prio_buf, "normal", size);
        }
        fclose(f);
    } else {
        strncpy(prio_buf, "normal", size);
    }
}

// Save priority to meta file
static void set_note_priority(const char *name, const char *prio) {
    char dir[PATH_MAX_LEN], meta_path[PATH_MAX_LEN];
    get_notes_dir(dir, sizeof(dir));
    snprintf(meta_path, sizeof(meta_path), "%s/%s.meta", dir, name);

    FILE *f = fopen(meta_path, "w");
    if (f) {
        fprintf(f, "%s\n", prio);
        fclose(f);
    }
}

// Auto-save buffer to note file
static void save_note_file(void) {
    if (current_note_name[0] == '\0') return;

    char dir[PATH_MAX_LEN], file_path[PATH_MAX_LEN];
    get_notes_dir(dir, sizeof(dir));
    snprintf(file_path, sizeof(file_path), "%s/%s.md", dir, current_note_name);

    FILE *f = fopen(file_path, "w");
    if (f) {
        fwrite(text_buf, 1, buf_len, f);
        fclose(f);
    }
}

// Catch Ctrl+C or kill signals, save, restore terminal and exit
static void handle_signal(int sig) {
    (void)sig;
    save_note_file();
    disable_raw_mode();
    exit(0);
}

// Color helper for priority tags
static const char* get_priority_color(const char *prio) {
    if (strcmp(prio, "urgent") == 0) return "\033[1;31m";   // Bright Red
    if (strcmp(prio, "normal") == 0) return "\033[1;36m";   // Bright Cyan
    if (strcmp(prio, "can-wait") == 0) return "\033[1;32m"; // Bright Green
    if (strcmp(prio, "maybe") == 0) return "\033[1;33m";    // Bright Yellow
    return "\033[1;35m";                                    // Custom: Bright Magenta
}

// Render Markdown styled text inside the editor
static void print_markdown_line(const char *line, int max_chars) {
    int printed = 0;
    
    // Header styling (# Header)
    if (line[0] == '#') {
        printf("\033[1;35m"); // Bright Magenta
    } 
    // Bullet list (- item or * item)
    else if ((line[0] == '-' || line[0] == '*') && line[1] == ' ') {
        printf("\033[1;33m• \033[0m");
        line += 2;
        printed += 2;
    }

    int in_bold = 0, in_code = 0;
    for (int i = 0; line[i] != '\0' && printed < max_chars; i++) {
        // Simple Bold toggle (**text**)
        if (line[i] == '*' && line[i+1] == '*') {
            in_bold = !in_bold;
            printf(in_bold ? "\033[1m" : "\033[22m");
            i++;
            continue;
        }
        // Inline Code toggle (`code`)
        if (line[i] == '`') {
            in_code = !in_code;
            printf(in_code ? "\033[33;40m" : "\033[0m");
            continue;
        }
        putchar(line[i]);
        printed++;
    }
    printf("\033[0m"); // Reset ANSI styling
}

// Draw centered TUI box in the terminal window
static void draw_editor_ui(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        ws.ws_col = 80;
        ws.ws_row = 24;
    }

    int box_w = ws.ws_col > 70 ? 66 : ws.ws_col - 4;
    int box_h = ws.ws_row > 18 ? 16 : ws.ws_row - 4;
    if (box_w < 20) box_w = 20;
    if (box_h < 5) box_h = 5;

    int margin_x = (ws.ws_col - box_w) / 2;
    int margin_y = (ws.ws_row - box_h) / 2;

    printf("\033[2J"); // Clear screen

    char prio[32];
    get_note_priority(current_note_name, prio, sizeof(prio));

    // Top border
    printf("\033[%d;%dH\033[1;34m┌─ Note: %s (%s%s\033[1;34m) ── [Ctrl+C to Save & Exit] ─", 
           margin_y, margin_x, current_note_name, get_priority_color(prio), prio);
    for (int i = 0; i < box_w - 45 - (int)strlen(current_note_name) - (int)strlen(prio); i++) {
        fputs("─", stdout);
    }
    printf("┐\033[0m");

    // Calculate line breaks and render text inside box
    int cur_line = 0;
    int cursor_screen_r = margin_y + 1, cursor_screen_c = margin_x + 2;

    int content_rows = box_h - 2;
    int content_cols = box_w - 4;

    int idx = 0;
    while (idx <= buf_len && cur_line < content_rows) {
        printf("\033[%d;%dH\033[1;34m│\033[0m ", margin_y + 1 + cur_line, margin_x);

        char line_buf[512] = {0};
        int l_idx = 0;

        while (idx < buf_len && text_buf[idx] != '\n' && l_idx < content_cols) {
            if (idx == cursor_pos) {
                cursor_screen_r = margin_y + 1 + cur_line;
                cursor_screen_c = margin_x + 2 + l_idx;
            }
            line_buf[l_idx++] = text_buf[idx++];
        }

        if (idx == cursor_pos) {
            cursor_screen_r = margin_y + 1 + cur_line;
            cursor_screen_c = margin_x + 2 + l_idx;
        }

        print_markdown_line(line_buf, content_cols);

        // Fill remaining spaces inside box row
        for (int i = l_idx; i < content_cols + 1; i++) putchar(' ');
        printf("\033[1;34m│\033[0m");

        if (idx < buf_len && text_buf[idx] == '\n') idx++;
        cur_line++;
    }

    // Fill remaining empty rows in the box
    while (cur_line < content_rows) {
        printf("\033[%d;%dH\033[1;34m│\033[0m", margin_y + 1 + cur_line, margin_x);
        for (int i = 0; i < content_cols + 2; i++) putchar(' ');
        printf("\033[1;34m│\033[0m");
        cur_line++;
    }

    // Bottom border
    printf("\033[%d;%dH\033[1;34m└", margin_y + box_h - 1, margin_x);
    for (int i = 0; i < box_w - 2; i++) fputs("─", stdout);
    printf("┘\033[0m");

    // Position cursor at user typing location
    printf("\033[%d;%dH\033[?25h", cursor_screen_r, cursor_screen_c);
    fflush(stdout);
}

// Open and edit note in centered interactive UI
static void open_editor(const char *name) {
    ensure_notes_dir();
    strncpy(current_note_name, name, sizeof(current_note_name) - 1);

    // Read existing file content if available
    char dir[PATH_MAX_LEN], file_path[PATH_MAX_LEN];
    get_notes_dir(dir, sizeof(dir));
    snprintf(file_path, sizeof(file_path), "%s/%s.md", dir, name);

    FILE *f = fopen(file_path, "r");
    if (f) {
        buf_len = fread(text_buf, 1, MAX_BUF - 1, f);
        text_buf[buf_len] = '\0';
        fclose(f);
    } else {
        buf_len = 0;
        text_buf[0] = '\0';
    }
    cursor_pos = buf_len;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    atexit(disable_raw_mode);

    enable_raw_mode();

    while (1) {
        draw_editor_ui();

        char c;
        if (read(STDIN_FILENO, &c, 1) <= 0) continue;

        if (c == 3 || c == 24) { // Ctrl+C or Ctrl+X
            save_note_file();
            break;
        } else if (c == 127 || c == 8) { // Backspace
            if (cursor_pos > 0) {
                memmove(&text_buf[cursor_pos - 1], &text_buf[cursor_pos], buf_len - cursor_pos);
                cursor_pos--;
                buf_len--;
                text_buf[buf_len] = '\0';
            }
        } else if (c == 13 || c == 10) { // Enter
            if (buf_len < MAX_BUF - 1) {
                memmove(&text_buf[cursor_pos + 1], &text_buf[cursor_pos], buf_len - cursor_pos);
                text_buf[cursor_pos] = '\n';
                cursor_pos++;
                buf_len++;
                text_buf[buf_len] = '\0';
            }
        } else if (c == 27) { // Escape sequences (Arrow keys)
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) > 0 && read(STDIN_FILENO, &seq[1], 1) > 0) {
                if (seq[0] == '[') {
                    if (seq[1] == 'D' && cursor_pos > 0) cursor_pos--; // Left
                    if (seq[1] == 'C' && cursor_pos < buf_len) cursor_pos++; // Right
                }
            }
        } else if (isprint(c)) { // Regular character input
            if (buf_len < MAX_BUF - 1) {
                memmove(&text_buf[cursor_pos + 1], &text_buf[cursor_pos], buf_len - cursor_pos);
                text_buf[cursor_pos] = c;
                cursor_pos++;
                buf_len++;
                text_buf[buf_len] = '\0';
            }
        }
    }
}

// Command: note -list (List all notes and their formatted contents)
static void cmd_list(void) {
    ensure_notes_dir();
    char dir_path[PATH_MAX_LEN];
    get_notes_dir(dir_path, sizeof(dir_path));

    DIR *d = opendir(dir_path);
    if (!d) {
        printf("No notes found.\n");
        return;
    }

    struct dirent *dir;
    int count = 0;
    printf("\033[1;36m=== YOUR NOTES ===\033[0m\n\n");

    while ((dir = readdir(d)) != NULL) {
        char *ext = strrchr(dir->d_name, '.');
        if (ext && strcmp(ext, ".md") == 0) {
            count++;
            char name[256];
            size_t len = ext - dir->d_name;
            strncpy(name, dir->d_name, len);
            name[len] = '\0';

            char prio[32];
            get_note_priority(name, prio, sizeof(prio));

            printf("\033[1;33m📌 %s \033[0m[%s%s\033[0m]\n", name, get_priority_color(prio), prio);
            printf("\033[90m----------------------------------------\033[0m\n");

            char file_path[PATH_MAX_LEN];
            snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, dir->d_name);
            FILE *f = fopen(file_path, "r");
            if (f) {
                char line[512];
                while (fgets(line, sizeof(line), f)) {
                    printf("  ");
                    print_markdown_line(line, 80);
                }
                fclose(f);
            }
            printf("\n\n");
        }
    }
    closedir(d);

    if (count == 0) {
        printf("  \033[90m(No notes created yet. Use 'note <name>' to create one!)\033[0m\n\n");
    }
}

// Command: note -reminder (Appends 'note -list' to ~/.bashrc or ~/.zshrc)
static void cmd_reminder(void) {
    const char *home = getenv("HOME");
    if (!home) return;

    char rc_path[PATH_MAX_LEN];
    snprintf(rc_path, sizeof(rc_path), "%s/.zshrc", home);
    if (access(rc_path, F_OK) != 0) {
        snprintf(rc_path, sizeof(rc_path), "%s/.bashrc", home);
    }

    FILE *f = fopen(rc_path, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "note -list")) {
                fclose(f);
                printf("\033[32m[✓] Reminder is already active in %s!\033[0m\n", rc_path);
                return;
            }
        }
        fclose(f);
    }

    f = fopen(rc_path, "a");
    if (f) {
        fprintf(f, "\n# Auto-display notes on shell startup\nnote -list\n");
        fclose(f);
        printf("\033[32m[✓] Reminder enabled! Added 'note -list' to %s\033[0m\n", rc_path);
    }
}

// Command: note -create-priority <priority_name>
static void cmd_create_priority(const char *prio_name) {
    ensure_notes_dir();
    char dir[PATH_MAX_LEN], path[PATH_MAX_LEN];
    get_notes_dir(dir, sizeof(dir));
    snprintf(path, sizeof(path), "%s/.custom_priorities", dir);

    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "%s\n", prio_name);
        fclose(f);
        printf("\033[32m[✓] Custom priority '%s' created successfully!\033[0m\n", prio_name);
    }
}

// Command: note -priority <note_name> <priority_level>
static void cmd_set_priority(const char *note_name, const char *prio_level) {
    ensure_notes_dir();
    set_note_priority(note_name, prio_level);
    printf("\033[32m[✓] Priority for '%s' set to '%s'!\033[0m\n", note_name, prio_level);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage:\n");
        printf("  note <name>                           - Create/Edit note\n");
        printf("  note -list                            - List all notes\n");
        printf("  note -reminder                        - Show notes on shell start\n");
        printf("  note -priority <note> <level>         - Set priority\n");
        printf("  note -create-priority <level_name>    - Create custom priority\n");
        return 0;
    }

    if (strcmp(argv[1], "-list") == 0) {
        cmd_list();
    } else if (strcmp(argv[1], "-reminder") == 0) {
        cmd_reminder();
    } else if (strcmp(argv[1], "-create-priority") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: note -create-priority <priority_name>\n");
            return 1;
        }
        cmd_create_priority(argv[2]);
    } else if (strcmp(argv[1], "-priority") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: note -priority <note_name> <priority_level>\n");
            return 1;
        }
        cmd_set_priority(argv[2], argv[3]);
    } else {
        open_editor(argv[1]);
    }

    return 0;
}
