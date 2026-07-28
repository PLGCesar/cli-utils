/*
 * ip-info.c - Simple CLI tool to inspect network setup.
 * Supports: Linux, macOS, Windows, and Android (Termux/NDK).
 * Zero external heavy dependencies!
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* --- OS & Socket Compatibility Layer --- */
#if defined(__ANDROID__)
    #define OS_ANDROID 1
    #define OS_UNIX 1
    #include <unistd.h>
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
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "iphlpapi.lib")
#else
    #define OS_UNIX 1
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <ifaddrs.h>
    #include <net/if.h>
    #include <sys/utsname.h>
#endif

// Fire up sockets for Windows dev environment
static void net_init(void) {
#ifdef OS_WINDOWS
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[-] Failed to spin up Winsock. RIP.\n");
        exit(1);
    }
#endif
}

// Socket cleanup before rage quitting
static void net_cleanup(void) {
#ifdef OS_WINDOWS
    WSACleanup();
#endif
}

// Helper to close sockets cleanly across platforms
static void close_socket(int fd) {
#ifdef OS_WINDOWS
    closesocket(fd);
#else
    close(fd);
#endif
}

/* --- OS Detection --- */
static void print_os_info(void) {
    printf("[+] Target OS:\n");
#if defined(OS_ANDROID)
    printf("  └─ OS: Android 🤖 (Termux / Bionic Environment)\n");
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

/* --- Public IP (WAN) via HTTP GET --- */
static void print_public_ip(void) {
    printf("\n[+] Public IP (WAN Lookup):\n");

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo("api.ipify.org", "80", &hints, &res) != 0) {
        printf("  └─ [!] DNS resolution failed for ipify host (You offline bro?)\n");
        return;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        printf("  └─ [!] Socket allocation failed\n");
        freeaddrinfo(res);
        return;
    }

    if (connect(sock, res->ai_addr, (int)res->ai_addrlen) < 0) {
        printf("  └─ [!] Could not hit ipify server\n");
        close_socket(sock);
        freeaddrinfo(res);
        return;
    }

    freeaddrinfo(res);

    const char *http_req = "GET / HTTP/1.1\r\nHost: api.ipify.org\r\nUser-Agent: ip-info-cli\r\nConnection: close\r\n\r\n";
    send(sock, http_req, (int)strlen(http_req), 0);

    char buffer[1024] = {0};
    int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    close_socket(sock);

    if (bytes > 0) {
        char *body = strstr(buffer, "\r\n\r\n");
        if (body) {
            body += 4;
            char *end = body + strlen(body) - 1;
            while (end > body && (*end == '\r' || *end == '\n' || *end == ' ')) {
                *end = '\0';
                end--;
            }
            printf("  └─ Public IPv4: %s\n", body);
        } else {
            printf("  └─ [!] Unhandled HTTP payload format\n");
        }
    } else {
        printf("  └─ [!] Zero bytes read from socket\n");
    }
}

/* --- Private IPs (LAN Interfaces) --- */
static void print_private_ips(void) {
    printf("\n[+] Private IPs (Local NICs):\n");

#ifdef OS_UNIX
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        printf("  └─ [!] Failed to dump interfaces\n");
        return;
    }

    int count = 0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            char host[INET_ADDRSTRLEN];
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &(sa->sin_addr), host, sizeof(host));
            
            if (strcmp(ifa->ifa_name, "lo") == 0 || strcmp(ifa->ifa_name, "lo0") == 0) {
                printf("  ├─ %-12s : %s (loopback)\n", ifa->ifa_name, host);
            } else {
                printf("  ├─ %-12s : %s\n", ifa->ifa_name, host);
            }
            count++;
        }
    }
    if (count == 0) printf("  └─ [!] No active IPv4 NICs found\n");
    freeifaddrs(ifaddr);

#elif defined(OS_WINDOWS)
    IP_ADAPTER_ADDRESSES *adapters = NULL;
    ULONG outBufLen = 15360;
    adapters = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);

    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, adapters, &outBufLen) == NO_ERROR) {
        IP_ADAPTER_ADDRESSES *curr = adapters;
        int count = 0;
        while (curr) {
            IP_ADAPTER_UNICAST_ADDRESS *unicast = curr->FirstUnicastAddress;
            while (unicast) {
                struct sockaddr_in *sa = (struct sockaddr_in *)unicast->Address.lpSockaddr;
                char host[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(sa->sin_addr), host, sizeof(host));
                
                printf("  ├─ %-12ls : %s\n", curr->FriendlyName, host);
                count++;
                unicast = unicast->Next;
            }
            curr = curr->Next;
        }
        if (count == 0) printf("  └─ [!] No active adapters found\n");
    } else {
        printf("  └─ [!] Failed to dump Windows adapters\n");
    }
    free(adapters);
#endif
}

/* --- DNS Resolver Info --- */
static void print_dns_info(void) {
    printf("\n[+] System DNS Resolvers:\n");

#if defined(OS_ANDROID)
    // Sniff Android system properties for DNS
    FILE *pipe = popen("getprop net.dns1", "r");
    if (pipe) {
        char dns_buf[64] = {0};
        if (fgets(dns_buf, sizeof(dns_buf), pipe) && strlen(dns_buf) > 1) {
            dns_buf[strcspn(dns_buf, "\r\n")] = 0;
            printf("  ├─ DNS (Android Prop): %s\n", dns_buf);
            pclose(pipe);
            return;
        }
        pclose(pipe);
    }
#endif

#ifdef OS_UNIX
    FILE *fp = fopen("/etc/resolv.conf", "r");
    if (!fp) {
        printf("  └─ [!] Unable to read /etc/resolv.conf\n");
        return;
    }

    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "nameserver", 10) == 0) {
            char dns_ip[128];
            if (sscanf(line, "nameserver %127s", dns_ip) == 1) {
                printf("  ├─ DNS: %s\n", dns_ip);
                count++;
            }
        }
    }
    fclose(fp);
    if (count == 0) printf("  └─ [!] No nameserver entries found\n");

#elif defined(OS_WINDOWS)
    FIXED_INFO *pFixedInfo = (FIXED_INFO *)malloc(sizeof(FIXED_INFO));
    ULONG ulOutBufLen = sizeof(FIXED_INFO);

    if (GetNetworkParams(pFixedInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
        free(pFixedInfo);
        pFixedInfo = (FIXED_INFO *)malloc(ulOutBufLen);
    }

    if (GetNetworkParams(pFixedInfo, &ulOutBufLen) == NO_ERROR) {
        IP_ADDR_STRING *pIPAddr = &pFixedInfo->DnsServerList;
        int count = 0;
        while (pIPAddr) {
            if (strlen(pIPAddr->IpAddress.String) > 0) {
                printf("  ├─ DNS: %s\n", pIPAddr->IpAddress.String);
                count++;
            }
            pIPAddr = pIPAddr->Next;
        }
        if (count == 0) printf("  └─ [!] No DNS servers listed\n");
    } else {
        printf("  └─ [!] Failed to fetch Windows DNS params\n");
    }
    free(pFixedInfo);
#endif
}

/* --- Proxy Envs Check --- */
static void print_proxy_info(void) {
    printf("\n[+] Environment Proxy Settings:\n");

    const char *proxy_vars[] = {
        "HTTP_PROXY", "http_proxy",
        "HTTPS_PROXY", "https_proxy",
        "ALL_PROXY", "all_proxy",
        "NO_PROXY", "no_proxy"
    };

    int count = 0;
    size_t total_vars = sizeof(proxy_vars) / sizeof(proxy_vars[0]);

    for (size_t i = 0; i < total_vars; i++) {
        char *val = getenv(proxy_vars[i]);
        if (val && strlen(val) > 0) {
            printf("  ├─ %-12s : %s\n", proxy_vars[i], val);
            count++;
        }
    }

    if (count == 0) {
        printf("  └─ [x] No proxy env vars set (Direct connection setup)\n");
    }
}

/* --- 🔥 EXPERT MODE FUNCTIONS 🔥 --- */

static void print_expert_sys_info(void) {
    printf("\n-----------------------------------------\n");
    printf("🔥 EXPERT MODE - System & Kernel Info 🔥\n");
    printf("-----------------------------------------\n");

#ifdef OS_UNIX
    struct utsname uts;
    if (uname(&uts) == 0) {
        printf("  ├─ System Name : %s\n", uts.sysname);
        printf("  ├─ Hostname    : %s\n", uts.nodename);
        printf("  ├─ Kernel Rel  : %s\n", uts.release);
        printf("  └─ Arch        : %s\n", uts.machine);
    }
#elif defined(OS_WINDOWS)
    printf("  ├─ Platform    : Windows Win32 API\n");
    char *arch = getenv("PROCESSOR_ARCHITECTURE");
    printf("  └─ Architecture: %s\n", arch ? arch : "x86/x64");
#endif
}

static void print_expert_nics(void) {
    printf("\n🔥 EXPERT MODE - Advanced Subnet Analysis 🔥\n");

#ifdef OS_UNIX
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        printf("  └─ [!] Failed to fetch expert interfaces\n");
        return;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            char ip[INET_ADDRSTRLEN] = "N/A";
            char mask[INET_ADDRSTRLEN] = "N/A";

            struct sockaddr_in *sa_ip = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &(sa_ip->sin_addr), ip, sizeof(ip));

            if (ifa->ifa_netmask) {
                struct sockaddr_in *sa_mask = (struct sockaddr_in *)ifa->ifa_netmask;
                inet_ntop(AF_INET, &(sa_mask->sin_addr), mask, sizeof(mask));
            }

            printf("  ├─ Interface: %s\n", ifa->ifa_name);
            printf("  │  ├─ IPv4 Address : %s\n", ip);
            printf("  │  ├─ Subnet Mask  : %s\n", mask);
            printf("  │  └─ Interface Up : %s\n", (ifa->ifa_flags & IFF_UP) ? "YES" : "NO");
        }
    }
    freeifaddrs(ifaddr);

#elif defined(OS_WINDOWS)
    IP_ADAPTER_ADDRESSES *adapters = NULL;
    ULONG outBufLen = 15360;
    adapters = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);

    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, adapters, &outBufLen) == NO_ERROR) {
        IP_ADAPTER_ADDRESSES *curr = adapters;
        while (curr) {
            IP_ADAPTER_UNICAST_ADDRESS *unicast = curr->FirstUnicastAddress;
            while (unicast) {
                struct sockaddr_in *sa = (struct sockaddr_in *)unicast->Address.lpSockaddr;
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(sa->sin_addr), ip, sizeof(ip));

                printf("  ├─ Interface: %ls\n", curr->FriendlyName);
                printf("  │  ├─ IPv4 Address : %s\n", ip);
                if (curr->PhysicalAddressLength > 0) {
                    printf("  │  ├─ MAC Address  : %02X:%02X:%02X:%02X:%02X:%02X\n",
                           curr->PhysicalAddress[0], curr->PhysicalAddress[1],
                           curr->PhysicalAddress[2], curr->PhysicalAddress[3],
                           curr->PhysicalAddress[4], curr->PhysicalAddress[5]);
                }
                printf("  │  └─ Status       : %s\n", (curr->OperStatus == IfOperStatusUp) ? "UP" : "DOWN");
                unicast = unicast->Next;
            }
            curr = curr->Next;
        }
    }
    free(adapters);
#endif
}

static void print_expert_latency(void) {
    printf("\n🔥 EXPERT MODE - Latency Check (Cloudflare 1.1.1.1:53) 🔥\n");

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo("1.1.1.1", "53", &hints, &res) != 0) {
        printf("  └─ [!] Cannot resolve 1.1.1.1 for ping test\n");
        return;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        return;
    }

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

    if (conn_res == 0) {
        printf("  └─ TCP Handshake RTT : %.2f ms ⚡\n", ms);
    } else {
        printf("  └─ [!] TCP handshake connection timed out\n");
    }
}

/* --- Entry Point --- */
int main(void) {
    net_init();

    printf("=========================================\n");
    printf("        IP-INFO CLI TOOL v1.1            \n");
    printf("=========================================\n");

    print_os_info();
    print_public_ip();
    print_private_ips();
    print_dns_info();
    print_proxy_info();

    // Expert Mode Prompt ("Press 1")
    printf("\n-----------------------------------------\n");
    printf("[?] Press '1' + Enter for Expert Mode (or press Enter to exit): ");
    fflush(stdout);

    char input[16];
    if (fgets(input, sizeof(input), stdin)) {
        if (input[0] == '1') {
            print_expert_sys_info();
            print_expert_nics();
            print_expert_latency();
            printf("\n[+] Expert Mode completed. Stay safe!\n");
        }
    }

    printf("\n[+] Exiting nicely.\n");
    net_cleanup();
    return 0;
}
