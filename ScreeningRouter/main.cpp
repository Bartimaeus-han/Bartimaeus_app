#include <arpa/inet.h> // inet_ntop()
#include <ios>
#include <iostream>
#include <linux/if_packet.h> // sockaddr_ll struct
#include <net/ethernet.h>    // ETH_P_IP protocol constant
#include <net/if.h>          // if_nametoindex()
#include <netinet/in.h>      // htons()
#include <netinet/ip.h>      // L3 IPv4 header struct (iphdr)
#include <poll.h>            // poll(), struct pollfd
#include <sys/socket.h>      // socket(), AF_PACKET, SOCK_DGRAM
#include <unistd.h>          // close() system call

int main() {
    std::cout << std::unitbuf;

    // 1. eth0 전용 Raw socket 생성
    int ext_sock = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
    if (ext_sock < 0) {
        std::cerr << "[!] Failed to create ext_sock (Root privilege required)" << std::endl;
        return 1;
    }
    // 2. eth1 전용 Raw socket 생성 (Create raw socket for eth1)
    int dmz_sock = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
    if (dmz_sock < 0) {
        std::cerr << "[!] Failed to create dmz_sock (Root privilege required)" << std::endl;
        close(ext_sock);
        return 1;
    }

    std::cout << "[+] Sockets created successfully (ext_sock: " << ext_sock << ", dmz_sock: " << dmz_sock << ")" << std::endl;

    // 3. Network Interface index 조회
    unsigned int ext_ifindex = if_nametoindex("eth0");
    unsigned int dmz_ifindex = if_nametoindex("eth1");

    if (ext_ifindex == 0 || dmz_ifindex == 0) {
        std::cerr << "[!] Failed to find network interfaces (eth0 or eth1 not found)" << std::endl;
        close(ext_sock);
        close(dmz_sock);
        return 1;
    }

    std::cout << "[+] Interface identified - eth0(External): " << ext_ifindex << ", eth1(DMZ): " << dmz_ifindex << std::endl;

    // 4. 각 socket을 해당하는 network interface에 bind
    struct sockaddr_ll ext_sll{};
    ext_sll.sll_family = AF_PACKET;
    ext_sll.sll_protocol = htons(ETH_P_IP);
    ext_sll.sll_ifindex = ext_ifindex;
    if (bind(ext_sock, reinterpret_cast<struct sockaddr *>(&ext_sll), sizeof(ext_sll)) < 0) {
        std::cerr << "[!] Failed to bind ext_sock to eth0" << std::endl;
        close(ext_sock);
        close(dmz_sock);
        return 1;
    }

    struct sockaddr_ll dmz_sll{};
    dmz_sll.sll_family = AF_PACKET;
    dmz_sll.sll_protocol = htons(ETH_P_IP);
    dmz_sll.sll_ifindex = dmz_ifindex;
    if (bind(dmz_sock, reinterpret_cast<struct sockaddr *>(&dmz_sll), sizeof(dmz_sll)) < 0) {
        std::cerr << "[!] Failed to bind dmz_sock to eth1" << std::endl;
        close(ext_sock);
        close(dmz_sock);
        return 1;
    }

    std::cout << "[+] Sockets successfully bound to respective interfaces" << std::endl;

    // 5. I/O multiplexing을 위한 pollfd 구조체 배열 구성
    struct pollfd fds[2];
    fds[0].fd = ext_sock;
    fds[0].events = POLLIN; // eth0 수신 대기

    fds[1].fd = dmz_sock;
    fds[1].events = POLLIN; // eth1 수신 대기

    std::cout << "[*] Screening Router packet forwarding loop started..." << std::endl;

    // Buffer for receiving L3 packets
    char buffer[2048];

    while (true) {
        // kernel에 수신 event 대기 요청
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            std::cerr << "[!] poll() error occurred" << std::endl;
            break;
        }

        //
        if (fds[0].revents & POLLIN) {
            // Receive inbound packet from eth0
            struct sockaddr_ll src_sll{};
            socklen_t sll_len = sizeof(src_sll);
            ssize_t len = recvfrom(ext_sock, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&src_sll), &sll_len);
            if (len < 0) {
                std::cerr << "[!] recvfrom(ext_sock) failed" << std::endl;
                continue;
            }

            // eth0에서 감지된 패킷 중, 밖으로 나가는건 그냥 무시를 한다.
            if (src_sll.sll_pkttype == PACKET_OUTGOING)
                continue;

            // Parse L3 IPv4 header
            // SOCK_RAW를 사용했다면 MAC주소(14byte)가 먼저 들어오지만, 우리는 SOCK_DGRAM을 사용했기 때문에 상관X
            struct iphdr *ip_header = reinterpret_cast<struct iphdr *>(buffer);
            char src_ip[INET_ADDRSTRLEN];
            char dst_ip[INET_ADDRSTRLEN];

            inet_ntop(AF_INET, &(ip_header->saddr), src_ip, sizeof(src_ip));
            inet_ntop(AF_INET, &(ip_header->daddr), dst_ip, sizeof(dst_ip));

            std::cout << "[L3 Inbound] " << src_ip << " -> " << dst_ip << " (Proto: " << static_cast<int>(ip_header->protocol) << ")" << std::endl;

            sendto(dmz_sock, buffer, len, 0, reinterpret_cast<struct sockaddr *>(&dmz_sll), sizeof(dmz_sll));
        }

        if (fds[1].revents & POLLIN) {
            // Receive return packet from eth1
            struct sockaddr_ll dmz_src_sll{};
            socklen_t dmz_sll_len = sizeof(dmz_src_sll);
            ssize_t dmz_len = recvfrom(dmz_sock, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&dmz_src_sll), &dmz_sll_len);
            if (dmz_len < 0) {
                std::cerr << "[!] recvfrom(dmz_sock) failed" << std::endl;
                continue;
            }

            // eth1에서 감지된 패킷 중, 밖으로 나가는건 무시 (Ignore locally transmitted packets on eth1)
            if (dmz_src_sll.sll_pkttype == PACKET_OUTGOING)
                continue;

            // Parse L3 IPv4 header
            struct iphdr *ip_header = reinterpret_cast<struct iphdr *>(buffer);
            char src_ip[INET_ADDRSTRLEN];
            char dst_ip[INET_ADDRSTRLEN];

            inet_ntop(AF_INET, &(ip_header->saddr), src_ip, sizeof(src_ip));
            inet_ntop(AF_INET, &(ip_header->daddr), dst_ip, sizeof(dst_ip));

            // Loop Guard
            if (std::string(dst_ip) == "172.22.0.2")
                continue;

            std::cout << "[L3 Outbound] " << src_ip << " -> " << dst_ip << " (Proto: " << static_cast<int>(ip_header->protocol) << ")" << std::endl;

            // eth0(외부)으로 응답 패킷 포워딩 (Forward return packet to eth0/External)
            sendto(ext_sock, buffer, dmz_len, 0, reinterpret_cast<struct sockaddr *>(&ext_sll), sizeof(ext_sll));
        }
    }

    close(ext_sock);
    close(dmz_sock);

    return 0;
}
