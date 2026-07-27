/*
 *  Fast, cross-platform CLI calculator with zero heap allocation.
 *  Supports: +, -, x, *, ÷, /, √, ², ^, parentheses, negative numbers, and PEMDAS.
 *  Targeted for GCC (x86-64, aarch64, ARM MCUs, RISC-V, etc.).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// Portable timer hook (works on Linux, macOS, Windows, bare-metal fallback)
#if defined(_WIN32)
    #include <windows.h>
#elif defined(__unix__) || defined(__APPLE__) || defined(__POSIX__)
    #include <time.h>
#else
    #include <time.h>
#endif

// Grab high-precision timestamp in milliseconds
static inline double get_time_ms(void) {
#if defined(_WIN32)
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart * 1000.0 / (double)freq.QuadPart;
#elif defined(CLOCK_MONOTONIC)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
#else
    // Generic standard C fallback for bare-metal microcontrollers
    return ((double)clock() * 1000.0) / CLOCKS_PER_SEC;
#endif
}

// Error flag defines
#define ERR_OK          0
#define ERR_SYNTAX      1
#define ERR_DIV_ZERO    2
#define ERR_MATH_DOMAIN 3
#define ERR_UNBALANCED  4

// Nuke annoying whitespace
static inline void skip_ws(const char **s) {
    while (**s == ' ' || **s == '\t' || **s == '\r' || **s == '\n') {
        (*s)++;
    }
}

// Pre-declaring grammar rules to handle nested PEMDAS recursion smoothly
static double parse_expr(const char **s, int *err);
static double parse_term(const char **s, int *err);
static double parse_power(const char **s, int *err);
static double parse_unary(const char **s, int *err);
static double parse_postfix(const char **s, int *err);
static double parse_primary(const char **s, int *err);

// Primary parser: numbers, parentheses, and text functions like sqrt()
static double parse_primary(const char **s, int *err) {
    skip_ws(s);

    if (**s == '\0') {
        *err = ERR_SYNTAX;
        return 0.0;
    }

    // Handles parentheses - highest PEMDAS group priority
    if (**s == '(') {
        (*s)++;
        double val = parse_expr(s, err);
        skip_ws(s);
        if (**s == ')') {
            (*s)++;
        } else {
            *err = ERR_UNBALANCED; // Oops, user forgot to close bracket
        }
        return val;
    }

    // ASCII "sqrt(...)" support
    if (strncmp(*s, "sqrt", 4) == 0) {
        *s += 4;
        double val = parse_primary(s, err);
        if (val < 0.0) {
            *err = ERR_MATH_DOMAIN;
            return 0.0;
        }
        return sqrt(val);
    }

    // Snag a floating-point or integer number
    char *endptr;
    double val = strtod(*s, &endptr);
    if (*s == endptr) {
        *err = ERR_SYNTAX;
        return 0.0;
    }
    *s = endptr;
    return val;
}

// Postfix operators: ² (\xC2\xB2 in UTF-8)
static double parse_postfix(const char **s, int *err) {
    double val = parse_primary(s, err);
    if (*err) return 0.0;

    skip_ws(s);
    // Check UTF-8 byte match for '²'
    if ((unsigned char)(**s) == 0xC2 && (unsigned char)((*s)[1]) == 0xB2) {
        *s += 2;
        val = val * val;
    }
    return val;
}

// Unary operators: +, -, √ (\xE2\x88\x9A in UTF-8)
static double parse_unary(const char **s, int *err) {
    skip_ws(s);

    if (**s == '+') {
        (*s)++;
        return parse_unary(s, err);
    }
    if (**s == '-') {
        (*s)++;
        return -parse_unary(s, err); // Handles negative numbers
    }
    // Check UTF-8 byte match for '√'
    if ((unsigned char)(**s) == 0xE2 && (unsigned char)((*s)[1]) == 0x88 && (unsigned char)((*s)[2]) == 0x9A) {
        *s += 3;
        double val = parse_unary(s, err);
        if (val < 0.0) {
            *err = ERR_MATH_DOMAIN;
            return 0.0;
        }
        return sqrt(val);
    }

    return parse_postfix(s, err);
}

// Exponentiation operator '^' (right-associative)
static double parse_power(const char **s, int *err) {
    double left = parse_unary(s, err);
    if (*err) return 0.0;

    skip_ws(s);
    if (**s == '^') {
        (*s)++;
        double right = parse_power(s, err);
        if (*err) return 0.0;
        return pow(left, right);
    }
    return left;
}

// Multiplication & Division: *, x, X, /, ÷ (\xC3\xB7 in UTF-8)
static double parse_term(const char **s, int *err) {
    double left = parse_power(s, err);
    if (*err) return 0.0;

    while (1) {
        skip_ws(s);
        if (**s == '*' || **s == 'x' || **s == 'X') {
            (*s)++;
            double right = parse_power(s, err);
            if (*err) return 0.0;
            left *= right;
        } else if (**s == '/') {
            (*s)++;
            double right = parse_power(s, err);
            if (*err) return 0.0;
            if (right == 0.0) { *err = ERR_DIV_ZERO; return 0.0; }
            left /= right;
        } else if ((unsigned char)(**s) == 0xC3 && (unsigned char)((*s)[1]) == 0xB7) { // UTF-8 '÷'
            *s += 2;
            double right = parse_power(s, err);
            if (*err) return 0.0;
            if (right == 0.0) { *err = ERR_DIV_ZERO; return 0.0; }
            left /= right;
        } else {
            break;
        }
    }
    return left;
}

// Addition & Subtraction: +, - (Lowest operator precedence)
static double parse_expr(const char **s, int *err) {
    double left = parse_term(s, err);
    if (*err) return 0.0;

    while (1) {
        skip_ws(s);
        if (**s == '+') {
            (*s)++;
            double right = parse_term(s, err);
            if (*err) return 0.0;
            left += right;
        } else if (**s == '-') {
            (*s)++;
            double right = parse_term(s, err);
            if (*err) return 0.0;
            left -= right;
        } else {
            break;
        }
    }
    return left;
}

int main(int argc, char **argv) {
    char input[512];
    const char *expr_str = NULL;

    if (argc > 1) {
        // Read directly from argument if passed via CLI
        expr_str = argv[1];
    } else {
        printf("Enter expression: ");
        if (!fgets(input, sizeof(input), stdin)) {
            return 1;
        }
        expr_str = input;
    }

    const char *p = expr_str;
    int err = ERR_OK;

    // Start benchmarking clock
    double t_start = get_time_ms();

    // Do math magic
    double result = parse_expr(&p, &err);

    skip_ws(&p);
    if (*p != '\0' && !err) {
        err = ERR_SYNTAX; // Leftover unparsed trash in input
    }

    double t_end = get_time_ms();
    double time_taken = t_end - t_start;

    if (err) {
        switch (err) {
            case ERR_DIV_ZERO:
                fprintf(stderr, "Error: Division by zero!\n");
                break;
            case ERR_MATH_DOMAIN:
                fprintf(stderr, "Error: Square root of a negative number!\n");
                break;
            case ERR_UNBALANCED:
                fprintf(stderr, "Error: Unbalanced parentheses!\n");
                break;
            default:
                fprintf(stderr, "Error: Syntax error!\n");
                break;
        }
        return 1;
    }

    // Output formatting ANSI escape sequences:
    // \033[90m   = Gray text
    // \033[97;1m = Neon / Bright Bold White text
    // \033[0m    = Reset color
    printf("\033[90mOperation time: %.4f ms\033[0m\n", time_taken);
    printf("\033[97;1mResult of your operation: %g\033[0m\n", result);

    return 0;
}
