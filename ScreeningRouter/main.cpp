#include <arpa/inet.h> // ip/port 변환용 inet_ntop(), ntohs()
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <linux/if_packet.h> // Low-level packet address structure
#include <net/ethernet.h>    // ETH_P_IP constant
#include <net/if.h>          // if_nametoindex()
#include <netinet/in.h>      // for IPPROTO_ICMP
#include <netinet/ip.h>      // L3 IPv4 header struct
#include <netinet/ip_icmp.h> // ICMP header struct
#include <netinet/tcp.h>     // L4 TCP header struct
#include <sys/socket.h>      // for socket(), AF_INET, SOCK_RAW
#include <unistd.h>          // for close() function

int main() {
    std::cout << std::unitbuf;

    std::cout << "[ScreeningRouter] Initializing L3/L4 Raw Socket..." << std::endl;

    int raw_sock = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
    if (raw_sock < 0) {
        std::cerr << "[!] Failed to create raw socket (Root privilege required)" << std::endl;
        return 1;
    }
    std::cout << "[+] Raw socket create successfully (fd: " << raw_sock << ")" << std::endl;

    // change network interface name to index
    unsigned int ext_ifindex = if_nametoindex("eth0");
    unsigned int dmz_ifindex = if_nametoindex("eth1");
    if (ext_ifindex == 0 || dmz_ifindex == 0) {
        // error msg
    }

    std::cout << "[+] Interfaces bound - eth0(External): " << ext_ifindex << ", eth1(DMZ): " << dmz_ifindex << std::endl;

    char buffer[2048];
    std::cout << "[*] Listening for raw packet..." << std::endl;
    while (true) {
        struct sockaddr_ll sll;
        socklen_t sll_len = sizeof(sll);
        ssize_t data_size = recvfrom(raw_sock, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&sll), &sll_len);

        if (data_size < 0) {
            std::cerr << "[!] Failed to receive packet" << std::endl;

            continue;
        }
        std::cout << "[+] Packet captured! (Size: " << data_size << " bytes)" << std::endl;

        if (sll.sll_pkttype == PACKET_OUTGOING)
            continue;

        // 1. Parsing L3 IPv4 Header
        struct iphdr *ip_header = reinterpret_cast<struct iphdr *>(buffer);
        char src_ip[INET_ADDRSTRLEN];
        char dst_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(ip_header->saddr), src_ip, sizeof(src_ip));
        inet_ntop(AF_INET, &(ip_header->daddr), dst_ip, sizeof(dst_ip));

        // 3. Parse L4 header
        // IHL offset 만큼 skip
        // ipv4 헤더는 20~60byte까지 가변적인 길이를 가진다. 이를 ihl로 저장하여 표현
        int ip_header_len = ip_header->ihl * 4;

        // if TCP
        if (ip_header->protocol == IPPROTO_TCP) {
            // ip header 이후에 tcp header가 나오는 구조 이므로
            struct tcphdr *tcp_header = reinterpret_cast<struct tcphdr *>(buffer + ip_header_len);

            std::cout << "[TCP] " << src_ip << ":" << ntohs(tcp_header->source)
                      << " -> " << dst_ip << ":" << ntohs(tcp_header->dest) << std::endl;
        } else if (ip_header->protocol == IPPROTO_ICMP) {
            struct icmphdr *icmp_header = reinterpret_cast<struct icmphdr *>(buffer + ip_header_len);

            std::cout << "[ICMP] " << src_ip << " -> " << dst_ip
                      << " (Type: " << static_cast<int>(icmp_header->type) << ")" << std::endl;
        } else {
            std::cout << "[Other IP Protocol: " << static_cast<int>(ip_header->protocol) << "] " << src_ip << " -> " << dst_ip << std::endl;
        }

        unsigned int target_ifindex = (sll.sll_ifindex == ext_ifindex) ? dmz_ifindex : ext_ifindex;
        struct sockaddr_ll out_sll{};
        out_sll.sll_family = AF_PACKET;
        out_sll.sll_protocol = htons(ETH_P_IP);
        out_sll.sll_ifindex = target_ifindex;

        sendto(raw_sock, buffer, data_size, 0, reinterpret_cast<struct sockaddr *>(&out_sll), sizeof(out_sll));
    }

    close(raw_sock);

    return 0;
}