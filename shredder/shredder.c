/*
 * shredder.c - Secure File Destruction CLI Tool
 * Overwrites files multiple times with random garbage data before deletion to prevent recovery.
 * Supports: Linux, macOS, Windows, Android (Termux).
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS 1
    #include <windows.h>
    #include <io.h>
    #define fsync_file(fp) FlushFileBuffers((HANDLE)_get_osfhandle(fileno(fp)))
#else
    #define OS_UNIX 1
    #include <unistd.h>
    #define fsync_file(fp) fsync(fileno(fp))
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

/* --- Fill Buffer with Random Garbage Bytes --- */
static void fill_random_buffer(unsigned char *buf, size_t size) {
#ifdef OS_UNIX
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t read_bytes = fread(buf, 1, size, f);
        fclose(f);
        if (read_bytes == size) return;
    }
#endif
    // Fallback pseudo-random fill
    for (size_t i = 0; i < size; i++) {
        buf[i] = (unsigned char)(rand() % 256);
    }
}

/* --- Core Shredding Function --- */
static int shred_file(const char *filename, int passes, int zero_final, int verbose) {
    struct stat st;
    if (stat(filename, &st) != 0) {
        printf("%s[-] File not found: %s%s\n", C_RED, filename, C_RESET);
        return 0;
    }

    if (S_ISDIR(st.st_mode)) {
        printf("%s[-] '%s' is a directory! Shredder processes files only.%s\n", C_RED, filename, C_RESET);
        return 0;
    }

    off_t file_size = st.st_size;

    if (verbose) {
        printf("%s[+] Shredding '%s%s%s' (Size: %ld bytes, Passes: %d)%s\n",
               C_CYAN, C_BOLD, filename, C_CYAN, (long)file_size, passes, C_RESET);
    }

    if (file_size > 0) {
        #define CHUNK_SIZE 65536 // 64KB Chunk Buffer
        unsigned char buffer[CHUNK_SIZE];

        for (int pass = 1; pass <= passes; pass++) {
            FILE *fp = fopen(filename, "r+b");
            if (!fp) {
                printf("%s[-] Cannot open file for writing: %s%s\n", C_RED, filename, C_RESET);
                return 0;
            }

            fseek(fp, 0, SEEK_SET);
            off_t remaining = file_size;
            int is_zero_pass = (zero_final && pass == passes);

            while (remaining > 0) {
                size_t write_size = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : (size_t)remaining;

                if (is_zero_pass) {
                    memset(buffer, 0x00, write_size);
                } else {
                    fill_random_buffer(buffer, write_size);
                }

                if (fwrite(buffer, 1, write_size, fp) != write_size) {
                    printf("%s[-] Write error during pass %d%s\n", C_RED, pass, C_RESET);
                    fclose(fp);
                    return 0;
                }

                remaining -= write_size;
            }

            // Force flush data to physical storage media
            fflush(fp);
            fsync_file(fp);
            fclose(fp);

            if (verbose) {
                if (is_zero_pass) {
                    printf("  ├─ Pass %d/%d: %sZeroed out%s [✔]\n", pass, passes, C_YELLOW, C_RESET);
                } else {
                    printf("  ├─ Pass %d/%d: %sOverwritten with random garbage%s [✔]\n", pass, passes, C_YELLOW, C_RESET);
                }
            }
        }
    }

    // Truncate file to 0 bytes before unlinking
    FILE *truncate_fp = fopen(filename, "wb");
    if (truncate_fp) {
        fclose(truncate_fp);
    }

    // Unlink file from file system
    if (remove(filename) == 0) {
        printf("%s[✔] File '%s%s%s' completely shredded and destroyed!%s\n",
               C_GREEN, C_BOLD, filename, C_GREEN, C_RESET);
        return 1;
    } else {
        printf("%s[-] Failed to remove file: %s%s\n", C_RED, filename, C_RESET);
        return 0;
    }
}

/* --- Help Menu --- */
static void print_help(const char *prog_name) {
    printf("%sShredder - Secure File Destruction CLI Tool v1.0%s\n", C_BOLD, C_RESET);
    printf("Usage: %s [options] <file1> <file2> ...\n\n", prog_name);
    printf("Options:\n");
    printf("  -n, --passes <num>   Number of overwrite passes (default: 3)\n");
    printf("  -z, --zero           Final pass with zeros to hide shredding pattern\n");
    printf("  -q, --quiet          Suppress pass output\n");
    printf("  -h, --help           Display this help menu\n\n");
}

/* --- Main Entry Point --- */
int main(int argc, char *argv[]) {
    init_tty_check();
    srand((unsigned int)time(NULL));

    if (argc < 2) {
        print_help(argv[0]);
        return 0;
    }

    int passes = 3;
    int zero_final = 0;
    int verbose = 1;
    int file_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            verbose = 0;
        } else if (strcmp(argv[i], "-z") == 0 || strcmp(argv[i], "--zero") == 0) {
            zero_final = 1;
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--passes") == 0) {
            if (i + 1 < argc) {
                passes = atoi(argv[++i]);
                if (passes <= 0) passes = 3;
            }
        } else if (argv[i][0] == '-') {
            printf("%s[-] Unknown flag '%s'%s\n", C_RED, argv[i], C_RESET);
            print_help(argv[0]);
            return 1;
        } else {
            file_count++;
            shred_file(argv[i], passes, zero_final, verbose);
        }
    }

    if (file_count == 0) {
        printf("%s[-] No files specified for shredding.%s\n", C_RED, C_RESET);
        print_help(argv[0]);
        return 1;
    }

    return 0;
}
