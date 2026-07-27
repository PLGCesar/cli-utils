/*
 * markwarp.c - Fast directory bookmark manager in C.
 * Takes the 'pwd' (current directory) or a given path and pairs it with a name tag in ~/.marks.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MARKS_FILE ".marks"

// Helper to construct the path to ~/.marks
static void get_marks_filepath(char *buf, size_t size) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buf, size, "%s/%s", home, MARKS_FILE);
}

// Save or update a mark ( mark <name> [optional_path] )
static int cmd_mark(const char *name, const char *path_arg) {
    char abs_path[PATH_MAX];

    // If an explicit path is passed, resolve it; otherwise grab pwd via getcwd()
    if (path_arg && path_arg[0] != '\0') {
        if (!realpath(path_arg, abs_path)) {
            perror("realpath error");
            return 1;
        }
    } else {
        if (!getcwd(abs_path, sizeof(abs_path))) {
            perror("getcwd error");
            return 1;
        }
    }

    char filepath[PATH_MAX];
    get_marks_filepath(filepath, sizeof(filepath));

    // Atomic update using a temporary file to avoid file corruption
    char temp_filepath[PATH_MAX];
    snprintf(temp_filepath, sizeof(temp_filepath), "%s.tmp", filepath);

    FILE *f = fopen(filepath, "r");
    FILE *out = fopen(temp_filepath, "w");
    if (!out) {
        perror("Failed to open temp file");
        if (f) fclose(f);
        return 1;
    }

    int updated = 0;
    if (f) {
        char line[PATH_MAX + 256];
        size_t name_len = strlen(name);

        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, name, name_len) == 0 && line[name_len] == '=') {
                fprintf(out, "%s=%s\n", name, abs_path);
                updated = 1;
            } else {
                fputs(line, out);
            }
        }
        fclose(f);
    }

    if (!updated) {
        fprintf(out, "%s=%s\n", name, abs_path);
    }
    fclose(out);

    if (rename(temp_filepath, filepath) != 0) {
        perror("Failed to update ~/.marks");
        return 1;
    }

    printf("\033[32mMarked '%s' -> %s\033[0m\n", name, abs_path);
    return 0;
}

// Fetch the saved directory path for warp ( get <name> )
static int cmd_get(const char *name) {
    char filepath[PATH_MAX];
    get_marks_filepath(filepath, sizeof(filepath));

    FILE *f = fopen(filepath, "r");
    if (!f) return 1;

    char line[PATH_MAX + 256];
    size_t name_len = strlen(name);

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0; // strip newlines
        if (strncmp(line, name, name_len) == 0 && line[name_len] == '=') {
            printf("%s\n", line + name_len + 1);
            fclose(f);
            return 0;
        }
    }

    fclose(f);
    return 1;
}

// List all saved marks stored in ~/.marks
static int cmd_list(void) {
    char filepath[PATH_MAX];
    get_marks_filepath(filepath, sizeof(filepath));

    FILE *f = fopen(filepath, "r");
    if (!f) {
        printf("No marks stored yet in ~/.marks\n");
        return 0;
    }

    char line[PATH_MAX + 256];
    printf("\033[1;36mSaved Marks (~/.marks):\033[0m\n");

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            printf("  \033[1;33m%-15s\033[0m -> %s\n", line, eq + 1);
        }
    }
    fclose(f);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return cmd_list();

    if (strcmp(argv[1], "mark") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: mark <name> [optional_path]\n");
            return 1;
        }
        return cmd_mark(argv[2], argc >= 4 ? argv[3] : NULL);
    } 
    else if (strcmp(argv[1], "get") == 0) {
        if (argc < 3) return 1;
        return cmd_get(argv[2]);
    } 
    else if (strcmp(argv[1], "list") == 0) {
        return cmd_list();
    }

    fprintf(stderr, "Invalid command\n");
    return 1;
}
