/*
 * ip-info.c v2.1 - Ultimate Cross-Platform Network CLI Tool
 * Defensive Socket Parsing & Safe JSON Extraction.
 * Supports: Linux, macOS, Windows, Android (Termux/NDK).
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* --- OS & Socket Compatibility Layer --- */
#if defined(__ANDROID__)
    #define OS_ANDROID 1
    #define OS_UNIX 1
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/time.h>
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <ifaddrs.h>
    #include <net/if.h>
    #include <sys/utsname.h>
#elif defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS 1
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #include <io.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "iphlpapi.lib")
#else
    #define OS_UNIX 1
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/time.h>
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <ifaddrs.h>
    #include <net/if.h>
    #include <sys/utsname.h>
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

typedef struct {
    char ip[64];
    char country[64];
    char city[64];
    char isp[128];
    int success;
} PublicGeoInfo;

static void net_init(void) {
#ifdef OS_WINDOWS
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[-] Failed to spin up Winsock. RIP.\n");
        exit(1);
    }
#endif
}

static void net_cleanup(void) {
#ifdef OS_WINDOWS
    WSACleanup();
#endif
}

static void close_socket(int fd) {
#ifdef OS_WINDOWS
    closesocket(fd);
#else
    close(fd);
#endif
}

static void set_socket_timeout(int sock, int seconds) {
#ifdef OS_WINDOWS
    DWORD timeout = seconds * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const void*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const void*)&tv, sizeof(tv));
#endif
}

/* --- Defensive Safe JSON Extractor --- */
static void extract_json_val(const char *json, const char *key, char *out, size_t max_len) {
    if (!json || !key || !out || max_len == 0) return;
    out[0] = '\0';

    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    const char *pos = strstr(json, search_key);
    if (!pos) return;

    pos += strlen(search_key);
    while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n') pos++;

    if (*pos == '"') {
        pos++;
        const char *end = strchr(pos, '"');
        if (end && end > pos) {
            size_t len = (size_t)(end - pos);
            if (len >= max_len) len = max_len - 1;
            strncpy(out, pos, len);
            out[len] = '\0';
        }
    } else {
        const char *end = pos;
        while (*end && *end != ',' && *end != '}' && *end != ']' && *end != '\r' && *end != '\n') end++;
        if (end > pos) {
            size_t len = (size_t)(end - pos);
            if (len >= max_len) len = max_len - 1;
            strncpy(out, pos, len);
            out[len] = '\0';
        }
    }
}

static PublicGeoInfo get_public_ip_geo(void) {
    PublicGeoInfo info = {0};
    strcpy(info.country, "Unknown");
    strcpy(info.city, "Unknown");
    strcpy(info.isp, "Unknown");

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo("ip-api.com", "80", &hints, &res) == 0) {
        int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock >= 0) {
            set_socket_timeout(sock, 3);
            if (connect(sock, res->ai_addr, (int)res->ai_addrlen) == 0) {
                const char *req = "GET /json/?fields=status,country,city,isp,query HTTP/1.1\r\nHost: ip-api.com\r\nUser-Agent: ip-info-cli\r\nConnection: close\r\n\r\n";
                send(sock, req, (int)strlen(req), 0);
                char buffer[2048] = {0};
                int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
                close_socket(sock);

                if (bytes > 0) {
                    buffer[bytes] = '\0'; // Safe null termination
                    char *body = strstr(buffer, "\r\n\r\n");
                    if (body) {
                        body += 4;
                        extract_json_val(body, "query", info.ip, sizeof(info.ip));
                        extract_json_val(body, "country", info.country, sizeof(info.country));
                        extract_json_val(body, "city", info.city, sizeof(info.city));
                        extract_json_val(body, "isp", info.isp, sizeof(info.isp));
                        if (strlen(info.ip) > 0) {
                            info.success = 1;
                            freeaddrinfo(res);
                            return info;
                        }
                    }
                }
            } else {
                close_socket(sock);
            }
        }
        freeaddrinfo(res);
    }

    // Fallback: api.ipify.org
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo("api.ipify.org", "80", &hints, &res) == 0) {
        int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock >= 0) {
            set_socket_timeout(sock, 3);
            if (connect(sock, res->ai_addr, (int)res->ai_addrlen) == 0) {
                const char *req = "GET / HTTP/1.1\r\nHost: api.ipify.org\r\nUser-Agent: ip-info-cli\r\nConnection: close\r\n\r\n";
                send(sock, req, (int)strlen(req), 0);
                char buffer[1024] = {0};
                int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
                close_socket(sock);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    char *body = strstr(buffer, "\r\n\r\n");
                    if (body) {
                        body += 4;
                        char *end = body + strlen(body) - 1;
                        while (end > body && (*end == '\r' || *end == '\n' || *end == ' ')) {
                            *end = '\0';
                            end--;
                        }
                        strncpy(info.ip, body, sizeof(info.ip) - 1);
                        info.success = 1;
                        freeaddrinfo(res);
                        return info;
                    }
                }
            } else {
                close_socket(sock);
            }
        }
        freeaddrinfo(res);
    }

    strcpy(info.ip, "Unavailable (Offline)");
    return info;
}

static double measure_latency_ms(void) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo("1.1.1.1", "53", &hints, &res) != 0) return -1.0;

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) { freeaddrinfo(res); return -1.0; }

    set_socket_timeout(sock, 2);

#ifdef OS_UNIX
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
#else
    DWORD start = GetTickCount();
#endif

    int conn_res = connect(sock, res->ai_addr, (int)res->ai_addrlen);

#ifdef OS_UNIX
    clock_gettime(CLOCK_MONOTONIC, &end);
    double ms = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
#else
    DWORD end = GetTickCount();
    double ms = (double)(end - start);
#endif

    close_socket(sock);
    freeaddrinfo(res);

    return (conn_res == 0) ? ms : -1.0;
}

static void print_os_info(void) {
    printf("%s[+] Target OS:%s\n", C_CYAN, C_RESET);
#if defined(OS_ANDROID)
    printf("  └─ OS: Android 🤖 (Termux / Bionic)\n");
#elif defined(__linux__)
    printf("  └─ OS: Linux 🐧\n");
#elif defined(__APPLE__) || defined(__MACH__)
    printf("  └─ OS: macOS 🍎\n");
#elif defined(_WIN32) || defined(_WIN64)
    printf("  └─ OS: Windows 🪟\n");
#else
    printf("  └─ OS: Generic POSIX / Unknown ❓\n");
#endif
}

static void print_public_info(PublicGeoInfo *geo) {
    printf("\n%s[+] Public IP & Geolocation (WAN):%s\n", C_CYAN, C_RESET);
    if (geo->success) {
        printf("  ├─ IP Address : %s%s%s%s\n", C_BOLD, C_GREEN, geo->ip, C_RESET);
        printf("  ├─ Location   : %s, %s\n", geo->city, geo->country);
        printf("  └─ ISP        : %s\n", geo->isp);
    } else {
        printf("  └─ IP Address : %s%s%s\n", C_RED, geo->ip, C_RESET);
    }
}

static void print_private_ips(void) {
    printf("\n%s[+] Private IPs (IPv4 & IPv6):%s\n", C_CYAN, C_RESET);

#ifdef OS_UNIX
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        printf("  └─ %s[!] Failed to dump interfaces%s\n", C_RED, C_RESET);
        return;
    }

    int count = 0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        int family = ifa->ifa_addr->sa_family;

        if (family == AF_INET || family == AF_INET6) {
            char host[INET6_ADDRSTRLEN];
            if (family == AF_INET) {
                struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
                inet_ntop(AF_INET, &(sa->sin_addr), host, sizeof(host));
            } else {
                struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)ifa->ifa_addr;
                inet_ntop(AF_INET6, &(sa6->sin6_addr), host, sizeof(host));
            }

            printf("  ├─ %-10s [%s] : %s%s%s\n",
                   ifa->ifa_name,
                   (family == AF_INET) ? "IPv4" : "IPv6",
                   C_YELLOW, host, C_RESET);
            count++;
        }
    }
    if (count == 0) printf("  └─ %s[!] No active NICs found%s\n", C_RED, C_RESET);
    freeifaddrs(ifaddr);

#elif defined(OS_WINDOWS)
    IP_ADAPTER_ADDRESSES *adapters = NULL;
    ULONG outBufLen = 15360;
    adapters = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);

    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, adapters, &outBufLen) == NO_ERROR) {
        IP_ADAPTER_ADDRESSES *curr = adapters;
        while (curr) {
            IP_ADAPTER_UNICAST_ADDRESS *unicast = curr->FirstUnicastAddress;
            while (unicast) {
                int family = unicast->Address.lpSockaddr->sa_family;
                char host[INET6_ADDRSTRLEN];
                if (family == AF_INET) {
                    struct sockaddr_in *sa = (struct sockaddr_in *)unicast->Address.lpSockaddr;
                    inet_ntop(AF_INET, &(sa->sin_addr), host, sizeof(host));
                } else if (family == AF_INET6) {
                    struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)unicast->Address.lpSockaddr;
                    inet_ntop(AF_INET6, &(sa6->sin6_addr), host, sizeof(host));
                }

                printf("  ├─ %-12ls [%s] : %s%s%s\n",
                       curr->FriendlyName,
                       (family == AF_INET) ? "IPv4" : "IPv6",
                       C_YELLOW, host, C_RESET);
                unicast = unicast->Next;
            }
            curr = curr->Next;
        }
    }
    free(adapters);
#endif
}

static void print_dns_info(void) {
    printf("\n%s[+] System DNS Resolvers:%s\n", C_CYAN, C_RESET);

#if defined(OS_ANDROID)
    FILE *pipe = popen("getprop net.dns1", "r");
    if (pipe) {
        char dns_buf[64] = {0};
        if (fgets(dns_buf, sizeof(dns_buf), pipe) && strlen(dns_buf) > 1) {
            dns_buf[strcspn(dns_buf, "\r\n")] = 0;
            printf("  ├─ DNS (Android Prop): %s%s%s\n", C_BOLD, dns_buf, C_RESET);
            pclose(pipe);
            return;
        }
        pclose(pipe);
    }
#endif

#ifdef OS_UNIX
    FILE *fp = fopen("/etc/resolv.conf", "r");
    if (fp) {
        char line[256];
        int count = 0;
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "nameserver", 10) == 0) {
                char dns_ip[128];
                if (sscanf(line, "nameserver %127s", dns_ip) == 1) {
                    printf("  ├─ DNS: %s%s%s\n", C_BOLD, dns_ip, C_RESET);
                    count++;
                }
            }
        }
        fclose(fp);
        if (count > 0) return;
    }
    printf("  └─ %s[!] No nameservers found%s\n", C_RED, C_RESET);

#elif defined(OS_WINDOWS)
    FIXED_INFO *pFixedInfo = (FIXED_INFO *)malloc(sizeof(FIXED_INFO));
    ULONG ulOutBufLen = sizeof(FIXED_INFO);

    if (GetNetworkParams(pFixedInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
        free(pFixedInfo);
        pFixedInfo = (FIXED_INFO *)malloc(ulOutBufLen);
    }

    if (GetNetworkParams(pFixedInfo, &ulOutBufLen) == NO_ERROR) {
        IP_ADDR_STRING *pIPAddr = &pFixedInfo->DnsServerList;
        while (pIPAddr) {
            if (strlen(pIPAddr->IpAddress.String) > 0) {
                printf("  ├─ DNS: %s%s%s\n", C_BOLD, pIPAddr->IpAddress.String, C_RESET);
            }
            pIPAddr = pIPAddr->Next;
        }
    }
    free(pFixedInfo);
#endif
}

static void print_proxy_info(void) {
    printf("\n%s[+] Environment Proxy Settings:%s\n", C_CYAN, C_RESET);
    const char *proxy_vars[] = { "HTTP_PROXY", "http_proxy", "HTTPS_PROXY", "https_proxy", "ALL_PROXY", "all_proxy" };
    int count = 0;
    for (size_t i = 0; i < sizeof(proxy_vars)/sizeof(proxy_vars[0]); i++) {
        char *val = getenv(proxy_vars[i]);
        if (val && strlen(val) > 0) {
            printf("  ├─ %-12s : %s\n", proxy_vars[i], val);
            count++;
        }
    }
    if (count == 0) printf("  └─ %s[x] No proxy envs detected (Direct connection)%s\n", C_GREEN, C_RESET);
}

static void print_expert_mode(void) {
    printf("\n=========================================\n");
    printf("🔥 %sEXPERT MODE - System & Advanced Metrics%s 🔥\n", C_BOLD, C_RESET);
    printf("=========================================\n");

#ifdef OS_UNIX
    struct utsname uts;
    if (uname(&uts) == 0) {
        printf("  ├─ System      : %s (%s)\n", uts.sysname, uts.machine);
        printf("  ├─ Kernel Rel  : %s\n", uts.release);
        printf("  └─ Hostname    : %s\n", uts.nodename);
    }
#elif defined(OS_WINDOWS)
    printf("  ├─ Platform    : Windows Win32 API\n");
    printf("  └─ Architecture: %s\n", getenv("PROCESSOR_ARCHITECTURE"));
#endif

    double ms = measure_latency_ms();
    if (ms >= 0) {
        printf("  ├─ Cloudflare RTT Latency (1.1.1.1:53) : %s%.2f ms%s ⚡\n", C_GREEN, ms, C_RESET);
    } else {
        printf("  ├─ Cloudflare RTT Latency : %sTimeout / Blocked%s\n", C_RED, C_RESET);
    }
}

static void print_json_output(PublicGeoInfo *geo) {
    printf("{\n");
#if defined(OS_ANDROID)
    printf("  \"os\": \"Android\",\n");
#elif defined(__linux__)
    printf("  \"os\": \"Linux\",\n");
#elif defined(__APPLE__)
    printf("  \"os\": \"macOS\",\n");
#elif defined(OS_WINDOWS)
    printf("  \"os\": \"Windows\",\n");
#else
    printf("  \"os\": \"Unknown\",\n");
#endif

    printf("  \"public\": {\n");
    printf("    \"ip\": \"%s\",\n", geo->ip);
    printf("    \"country\": \"%s\",\n", geo->country);
    printf("    \"city\": \"%s\",\n", geo->city);
    printf("    \"isp\": \"%s\"\n", geo->isp);
    printf("  },\n");

    double ms = measure_latency_ms();
    printf("  \"latency_ms\": %.2f\n", ms);
    printf("}\n");
}

static void print_help(const char *prog_name) {
    printf("%sip-info CLI Tool v2.1%s\n", C_BOLD, C_RESET);
    printf("Usage: %s [options]\n\n", prog_name);
    printf("Options:\n");
    printf("  -e, --expert      Run Expert Mode directly without interactive prompt\n");
    printf("  -j, --json        Output all network details in valid JSON format\n");
    printf("  -q, --quiet       Print ONLY the public IP address (ideal for scripts)\n");
    printf("  -h, --help        Show this help message\n\n");
}

int main(int argc, char *argv[]) {
    init_tty_check();
    net_init();

    int opt_expert = 0;
    int opt_json = 0;
    int opt_quiet = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--expert") == 0) opt_expert = 1;
        else if (strcmp(argv[i], "-j") == 0 || strcmp(argv[i], "--json") == 0) opt_json = 1;
        else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) opt_quiet = 1;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            net_cleanup();
            return 0;
        }
    }

    PublicGeoInfo geo = get_public_ip_geo();

    if (opt_quiet) {
        printf("%s\n", geo.ip);
        net_cleanup();
        return 0;
    }

    if (opt_json) {
        print_json_output(&geo);
        net_cleanup();
        return 0;
    }

    printf("=========================================\n");
    printf("      %sIP-INFO CLI TOOL v2.1%s           \n", C_BOLD, C_RESET);
    printf("=========================================\n");

    print_os_info();
    print_public_info(&geo);
    print_private_ips();
    print_dns_info();
    print_proxy_info();

    if (opt_expert) {
        print_expert_mode();
    } else {
        printf("\n-----------------------------------------\n");
        printf("[?] Press '1' + Enter for Expert Mode (or press Enter to exit): ");
        fflush(stdout);

        char input[16];
        if (fgets(input, sizeof(input), stdin)) {
            if (input[0] == '1') {
                print_expert_mode();
            }
        }
    }

    printf("\n[+] Done. Exiting nicely.\n");
    net_cleanup();
    return 0;
}
