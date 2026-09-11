#include <arpa/inet.h> // inet_pton(), inet_ntop()
#include <chrono>      // system_clock, milliseconds
#include <cstdint>
#include <cstring> // memcpy()
#include <ctime>   // localtime_r
#include <iostream>
#include <linux/if_packet.h> // sockaddr_ll struct
#include <net/ethernet.h>    // ETH_P_IP protocol constant
#include <net/if.h>          // if_nametoindex()
#include <netinet/in.h>      // htons()
#include <netinet/ip.h>      // struct iphdr
#include <netinet/tcp.h>     // struct tcphdr
#include <poll.h>            // poll(), struct pollfd
#include <sys/socket.h>      // socket(), AF_PACKET, SOCK_DGRAM
#include <unistd.h>          // close() system call
#include <unordered_map>     //

// 밀리초 포함 실시간 타임스탬프 생성 함수 (Generate real-time timestamp with milliseconds)
std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    time_t t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    struct tm tm_info {};
    localtime_r(&t, &tm_info);

    char buf[32];
    snprintf(buf, sizeof(buf), "[%02d:%02d:%02d.%03ld]",
             tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, static_cast<long>(ms.count()));
    return buf;
}

// NPAT session entry struct
struct NatSession {
    // Origin client IP & port
    struct in_addr client_ip;
    uint16_t client_port;
};

// RFC 791 Standard IP Checksum calculation function
uint16_t
calculate_checksum(const void *data, size_t length) {
    const uint16_t *buf = static_cast<const uint16_t *>(data);
    uint32_t sum = 0;

    while (length > 1) {
        sum += *buf++;
        length -= 2;
    }

    if (length == 1) {
        sum += *reinterpret_cast<const uint8_t *>(buf); // 홀수 byte 처리
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16); // 캐리 bit 가산
    }

    return static_cast<uint16_t>(~sum); // 1의 보수 반환
}

// L4 TCP Pseudo-Header struct
// __attribute__((packed)) : struct 안에 padding byte를 넣지 않는다.
struct __attribute__((packed)) pseudo_header {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t reserved;
    uint8_t protocol;
    uint16_t tcp_length;
};

// TCP Checksum calculation function
uint16_t calculate_tcp_checksum(struct iphdr *ip_header, struct tcphdr *tcp_header) {
    uint16_t ip_header_len = ip_header->ihl * 4;
    // tcp header 앞에 ip header가 위치한다.
    uint16_t tcp_seg_len = ntohs(ip_header->tot_len) - ip_header_len;

    // 계산을 위한 pseudo header 정보 입력
    pseudo_header psh{};
    psh.src_ip = ip_header->saddr;
    psh.dst_ip = ip_header->daddr;
    psh.reserved = 0;
    psh.protocol = IPPROTO_TCP;
    psh.tcp_length = htons(tcp_seg_len);

    char pseudo_packet[2048];
    memcpy(pseudo_packet, &psh, sizeof(psh));

    tcp_header->check = 0; // 계산 전 초기화
    memcpy(pseudo_packet + sizeof(psh), tcp_header, tcp_seg_len);

    return calculate_checksum(pseudo_packet, sizeof(psh) + tcp_seg_len);
}

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
    // 1_external_net이 eth0 (10.10.0.2), 2_dmz_net이 eth1 (10.20.0.2)
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
    struct sockaddr_ll ext_sll {};
    ext_sll.sll_family = AF_PACKET;
    ext_sll.sll_protocol = htons(ETH_P_IP);
    ext_sll.sll_ifindex = ext_ifindex;
    if (bind(ext_sock, reinterpret_cast<struct sockaddr *>(&ext_sll), sizeof(ext_sll)) < 0) {
        std::cerr << "[!] Failed to bind ext_sock to eth0" << std::endl;
        close(ext_sock);
        close(dmz_sock);
        return 1;
    }

    struct sockaddr_ll dmz_sll {};
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

    char buffer[2048]; // packet 수신용

    struct in_addr original_client_ip {}; // 원본 Client IP 백업용

    std::unordered_map<uint16_t, NatSession> session_table;

    while (true) {
        // kernel에 수신 event 대기 요청
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            std::cerr << "[!] poll() error occurred" << std::endl;
            break;
        }

        //
        if (fds[0].revents & POLLIN) {
            // forwarding logic
            struct sockaddr_ll sll {};
            socklen_t sll_len = sizeof(sll);

            // eth1로부터 L3 IPv4 packet 수신
            ssize_t data_size = recvfrom(ext_sock, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&sll), &sll_len);

            // IPv4 헤더의 물리적 최소 크기를 검증.
            // 기형 패킷 차단 -> Buffer Over-read
            if (data_size < static_cast<ssize_t>(sizeof(struct iphdr)))
                continue; // 비정상 runt packet 폐기
            // Runt Packet : 통신 규격이 정한 최소 크기보다 작아서 정상적으로 처리할 수 없는 pakcet

            // screening router 자신이 송출한 packet의 loopback 방지
            if (sll.sll_pkttype == PACKET_OUTGOING)
                continue;

            // L3 IPv4 header mapping
            struct iphdr *ip_header = reinterpret_cast<struct iphdr *>(buffer);
            char src_ip[INET_ADDRSTRLEN];
            char dst_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(ip_header->saddr), src_ip, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &(ip_header->daddr), dst_ip, INET_ADDRSTRLEN);

            // 라우터 자신이 보낸 반사 패킷 루프백 차단
            struct in_addr router_ext_ip {};
            inet_pton(AF_INET, "10.10.0.2", &router_ext_ip);
            if (ip_header->saddr == router_ext_ip.s_addr)
                continue;

            // Log
            std::cout << get_timestamp() << " [eth0(Ext) -> Inbound] " << src_ip << " -> " << dst_ip << " (Proto: " << static_cast<int>(ip_header->protocol) << ", Size: " << data_size << " bytes)" << std::endl;

            // 2-1. DNAT: dst ip를 ReverseProxy ip로 변경
            struct in_addr target_ip {};
            inet_pton(AF_INET, "10.20.0.3", &target_ip);
            ip_header->daddr = target_ip.s_addr;

            // 2-2. SNAT: src ip를 screening router DMZ ip로 변경
            original_client_ip.s_addr = ip_header->saddr;

            struct in_addr router_dmz_if_ip {};
            inet_pton(AF_INET, "10.20.0.2", &router_dmz_if_ip);
            ip_header->saddr = router_dmz_if_ip.s_addr;

            // 3. IP Checksum 재계산
            ip_header->check = 0; // 과거 체크섬 값을 완전히 제거
            ip_header->check = calculate_checksum(ip_header, ip_header->ihl * 4);

            // 3-1. TCP인 경우 checksum 재계산
            if (ip_header->protocol == IPPROTO_TCP) {
                struct tcphdr *tcp_header = reinterpret_cast<struct tcphdr *>(buffer + (ip_header->ihl * 4));

                // Client 출발지 포트 추출 및 세션 등록
                uint16_t client_port = ntohs(tcp_header->source);
                session_table[client_port] = NatSession{original_client_ip, client_port};

                tcp_header->check = calculate_tcp_checksum(ip_header, tcp_header);
            }

            // 4. eth1로 packet 송출
            struct sockaddr_ll out_sll {};
            out_sll.sll_family = AF_PACKET;
            out_sll.sll_protocol = htons(ETH_P_IP);
            out_sll.sll_ifindex = dmz_ifindex;
            out_sll.sll_halen = ETH_ALEN; // MAC 주소 길이
            for (int i = 0; i < ETH_ALEN; ++i) {
                // L2 Broadcast 전송. 0xFF로 채우면 브릿지가 ReverseProxy에게 packet을 온전히 전송
                out_sll.sll_addr[i] = 0xFF;
            }

            sendto(dmz_sock, buffer, data_size, 0, reinterpret_cast<struct sockaddr *>(&out_sll), sizeof(out_sll));
        }

        if (fds[1].revents & POLLIN) {
            struct sockaddr_ll sll {};
            socklen_t sll_len = sizeof(sll);

            // eth1(DMZ)로부터 백엔드 응답 패킷 수신 (Receive backend response from eth1)
            ssize_t data_size = recvfrom(dmz_sock, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&sll), &sll_len);
            if (data_size < static_cast<ssize_t>(sizeof(struct iphdr)))
                continue; // 런트 패킷 폐기 (Discard runt packet)

            // dmz_sock 자신이 송출한 패킷 루프백 방지 (Prevent loopback of self-transmitted packets)
            if (sll.sll_pkttype == PACKET_OUTGOING)
                continue;

            // L3 IPv4 헤더 매핑 (Map L3 IPv4 header)
            struct iphdr *ip_header = reinterpret_cast<struct iphdr *>(buffer);
            char src_ip[INET_ADDRSTRLEN];
            char dst_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(ip_header->saddr), src_ip, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &(ip_header->daddr), dst_ip, INET_ADDRSTRLEN);

            std::cout << get_timestamp() << " [eth1(DMZ) -> Outbound] " << src_ip << " -> " << dst_ip << " (Proto: " << static_cast<int>(ip_header->protocol) << ", Size: " << data_size << " bytes)" << std::endl;

            // ReverseProxy(10.20.0.3)가 보낸 응답이 아니면 Drop (Drop if response is not from ReverseProxy)
            struct in_addr expected_proxy_ip {};
            inet_pton(AF_INET, "10.20.0.3", &expected_proxy_ip);
            if (ip_header->saddr != expected_proxy_ip.s_addr)
                continue;

            // 2. Reverse NAT: 출발지 IP를 스크리닝 라우터의 외부 IP로 복원 (Restore src IP to Screening Router external IP)
            struct in_addr router_ext_ip {};
            inet_pton(AF_INET, "10.10.0.2", &router_ext_ip);
            ip_header->saddr = router_ext_ip.s_addr;

            // 2-1. TCP session table 조회 및 dst IP 복원
            if (ip_header->protocol == IPPROTO_TCP) {
                struct tcphdr *tcp_header = reinterpret_cast<struct tcphdr *>(buffer + (ip_header->ihl * 4));
                uint16_t reply_port = ntohs(tcp_header->dest);
                auto it = session_table.find(reply_port);

                // 미등록 세션의 비인가 패킷은 Drop
                if (it == session_table.end())
                    continue;

                ip_header->daddr = it->second.client_ip.s_addr;
            }

            // 3. IP Checksum 재계산 (Recalculate IP Checksum)
            ip_header->check = 0;
            ip_header->check = calculate_checksum(ip_header, ip_header->ihl * 4);

            // 3-1. TCP인 경우 checksum 재계산 (Recalculate TCP checksum if TCP)
            if (ip_header->protocol == IPPROTO_TCP) {
                struct tcphdr *tcp_header = reinterpret_cast<struct tcphdr *>(buffer + (ip_header->ihl * 4));

                tcp_header->check = calculate_tcp_checksum(ip_header, tcp_header);
            }

            // 4. eth0로 packet 송출 (Transmit packet via eth0)
            struct sockaddr_ll out_sll {};
            out_sll.sll_family = AF_PACKET;
            out_sll.sll_protocol = htons(ETH_P_IP);
            out_sll.sll_ifindex = ext_ifindex;
            out_sll.sll_halen = ETH_ALEN;
            for (int i = 0; i < ETH_ALEN; ++i) {
                out_sll.sll_addr[i] = 0xFF;
            }

            sendto(ext_sock, buffer, data_size, 0, reinterpret_cast<struct sockaddr *>(&out_sll), sizeof(out_sll));
        }
    }

    close(ext_sock);
    close(dmz_sock);

    return 0;
}
