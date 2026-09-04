#define _CRT_SECURE_NO_WARNINGS
#include <cstdlib>
#include <cstring>
#include <iostream>

// Cross-platform socket headers
#ifndef _WIN32
#include <netinet/in.h> // for IPPROTO_ICMP
#include <sys/socket.h> // for socket(), AF_INET, SOCK_RAW
#include <unistd.h>     // for close() function
#endif

int main() {
    std::cout << std::unitbuf;

    std::cout << "[ScreeningRouter] Initializing L3/L4 Raw Socket..." << std::endl;

#ifndef _WIN32
    int raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (raw_sock < 0) {
        std::cerr << "[!] Failed to create raw socket (Root privilege required)" << std::endl;
        return 1;
    }
    std::cout << "[+] Raw socket create successfully (fd: " << raw_sock << ")" << std::endl;

    close(raw_sock);
#else
    std::cout << "[!] Windows kernel blocks raw packet capture. Run inside Linux/Docker." << std::endl;
#endif

    return 0;
}