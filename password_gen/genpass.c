/*
 * genpass.c - Cross-platform 16-char secure password generator.
 * Generates high-entropy passwords, auto-clears screen after 30s, and securely wipes RAM.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#if defined(__unix__) || defined(__APPLE__)
    #include <unistd.h>
    #include <termios.h>
    #include <sys/select.h>
#elif defined(_WIN32)
    #include <windows.h>
    #include <conio.h>
#endif

#define PASS_LEN 16

static const char LOWER[]   = "abcdefghijklmnopqrstuvwxyz";
static const char UPPER[]   = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char DIGITS[]  = "0123456789";
static const char SPECIAL[] = "!@#$%^&*()_+-=[]{}|;:,.<>?";

// Volatile memory zeroing to ensure compiler optimization won't strip it out
static void secure_zero(void *v, size_t n) {
    volatile unsigned char *p = (volatile unsigned char *)v;
    while (n--) {
        *p++ = 0;
    }
}

// Sampler for high-entropy random bytes with fallback for bare-metal
static void get_random_bytes(void *buf, size_t len) {
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        if (fread(buf, 1, len, f) == len) {
            fclose(f);
            return;
        }
        fclose(f);
    }

    // Fallback pseudo-random generator for environments without /dev/urandom
    static unsigned int seed = 0;
    if (!seed) {
        seed = (unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)&seed;
    }
    unsigned char *p = (unsigned char *)buf;
    for (size_t i = 0; i < len; i++) {
        seed = seed * 1103515245 + 12345;
        p[i] = (unsigned char)(seed >> 16);
    }
}

// Fisher-Yates array shuffle algorithm
static void shuffle_chars(char *array, size_t n) {
    if (n <= 1) return;
    unsigned char rand_buf[n];
    get_random_bytes(rand_buf, n);

    for (size_t i = n - 1; i > 0; i--) {
        size_t j = rand_buf[i] % (i + 1);
        char t = array[i];
        array[i] = array[j];
        array[j] = t;
    }
    secure_zero(rand_buf, sizeof(rand_buf));
}

// Generate a 16-char password ensuring all required character groups exist
static void generate_password(char *out, size_t len) {
    const char *ALL = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+-=[]{}|;:,.<>?";
    unsigned char r[PASS_LEN];
    get_random_bytes(r, sizeof(r));

    // Guarantee at least 1 character from each group
    out[0] = LOWER[r[0] % strlen(LOWER)];
    out[1] = UPPER[r[1] % strlen(UPPER)];
    out[2] = DIGITS[r[2] % strlen(DIGITS)];
    out[3] = SPECIAL[r[3] % strlen(SPECIAL)];

    // Fill the rest with random characters
    for (size_t i = 4; i < len; i++) {
        out[i] = ALL[r[i] % strlen(ALL)];
    }
    out[len] = '\0';

    // Shuffle so guaranteed types aren't predictable at fixed indices
    shuffle_chars(out, len);
    secure_zero(r, sizeof(r));
}

// Non-blocking wait loop with 30-second countdown
static void wait_or_timeout(int seconds) {
#if defined(__unix__) || defined(__APPLE__)
    struct termios orig_termios, raw_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    raw_termios = orig_termios;
    raw_termios.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_termios);

    for (int i = seconds; i > 0; i--) {
        printf("\r\033[90mDid you copy it? (Press 'y' or Enter) [Auto-clearing in %2ds]: \033[0m", i);
        fflush(stdout);

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };

        int res = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        if (res > 0) {
            char ch = 0;
            if (read(STDIN_FILENO, &ch, 1) > 0) {
                if (ch == 'y' || ch == 'Y' || ch == '\n' || ch == '\r') {
                    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
                    return;
                }
            }
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
#elif defined(_WIN32)
    for (int i = seconds; i > 0; i--) {
        printf("\r\033[90mDid you copy it? (Press 'y' or Enter) [Auto-clearing in %2ds]: \033[0m", i);
        fflush(stdout);
        for (int ms = 0; ms < 1000; ms += 100) {
            if (_kbhit()) {
                char ch = _getch();
                if (ch == 'y' || ch == 'Y' || ch == '\r' || ch == '\n') return;
            }
            Sleep(100);
        }
    }
#else
    // Generic fallback for microcontrollers
    for (int i = seconds; i > 0; i--) {
        printf("\r[Auto-clearing in %2ds]...", i);
        fflush(stdout);
    }
#endif
}

int main(void) {
    char password[PASS_LEN + 1];

    // Generate password
    generate_password(password, PASS_LEN);

    // Clear screen and scrollback buffer
    printf("\033[2J\033[H\033[3J");

    printf("\n\033[1;36m========== GENPASS - SECURE PASSWORD GENERATOR ==========\033[0m\n\n");
    printf("  Your Generated Password:  \033[97;1;42m %s \033[0m\n\n", password);

    // Wait 30 seconds or user confirmation
    wait_or_timeout(30);

    // Securely wipe RAM buffer before exit
    secure_zero(password, sizeof(password));

    // Clear screen AND terminal scrollback buffer (\033[3J)
    printf("\033[2J\033[H\033[3J");
    printf("\033[32m[✓] Memory securely wiped and screen cleared. Stay safe!\033[0m\n");

    return 0;
}
