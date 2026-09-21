#include <arpa/inet.h> // inet_pton(), inet_ntop()
#include <chrono>      // system_clock, milliseconds
#include <cstdint>
#include <cstring>   // memcpy()
#include <ctime>     // localtime_r
#include <ifaddrs.h> // getifaddrs(), freeifaddrs()
#include <iostream>
#include <linux/if_packet.h>  // sockaddr_ll struct
#include <net/ethernet.h>     // ETH_P_IP protocol constant
#include <net/if.h>           // if_nametoindex()
#include <netinet/if_ether.h> // struct ether_arp, ARPOP_REQUEST, ARPOP_REPLY
#include <netinet/in.h>       // htons()
#include <netinet/ip.h>       // struct iphdr
#include <netinet/tcp.h>      // struct tcphdr
#include <netinet/udp.h>      // struct udphdr
#include <poll.h>             // poll(), struct pollfd
#include <string>
#include <sys/ioctl.h>   // ioctl(), SIOCGIFHWADDR (나 자신 MAC 주소 조회용)
#include <sys/socket.h>  // socket(), AF_PACKET, SOCK_DGRAM
#include <unistd.h>      // close() system call
#include <unordered_map> //
#include <vector>

// 밀리초 포함 실시간 타임스탬프 생성 함수 (Generate real-time timestamp with milliseconds)
std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    time_t t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    struct tm tm_info{};
    localtime_r(&t, &tm_info);

    char buf[32];
    snprintf(buf, sizeof(buf), "[%02d:%02d:%02d.%03ld]",
             tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, static_cast<long>(ms.count()));
    return buf;
}

// ACL packet 처리 정책 enum
enum class Action {
    ALLOW,
    DENY
};

// L3/L4 packet 식별용 5-Tuple 구조체
struct FiveTuple {
    uint8_t protocol;
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
};

// L3/L4 ACL 규칙 구조체
struct AclRule {
    std::string rule_name; // 식별용 규칙 이름
    FiveTuple criteria;
    // Action on match
    Action action;
};

// ACL match result struct
struct AclMatchResult {
    Action action;
    // 룰 이름을 가지고 작업한다.
    std::string rule_name;
};

// L3/L4 5-Tuple 기반 ACL 순회 평가 함수 (Evaluate packet against ACL rules in First-Match order)
AclMatchResult evaluate_acl(const std::vector<AclRule> &rules, const FiveTuple &packet) {
    for (const auto &rule : rules) {
        // 1. 프로토콜 검사 (0이면 ANY로 건너뜀) (Protocol check - 0 means ANY)
        if (rule.criteria.protocol != 0 && rule.criteria.protocol != packet.protocol)
            continue;

        // 2. 출발지 IP 검사 (0이면 ANY로 건너뜀) (Source IP check - 0 means ANY)
        if (rule.criteria.src_ip != 0 && rule.criteria.src_ip != packet.src_ip)
            continue;

        // 3. 목적지 IP 검사 (0이면 ANY로 건너뜀) (Destination IP check - 0 means ANY)
        if (rule.criteria.dst_ip != 0 && rule.criteria.dst_ip != packet.dst_ip)
            continue;

        // 4. 출발지 포트 검사 (0이면 ANY로 건너뜀) (Source Port check - 0 means ANY)
        if (rule.criteria.src_port != 0 && rule.criteria.src_port != packet.src_port)
            continue;

        // 5. 목적지 포트 검사 (0이면 ANY로 건너뜀) (Destination Port check - 0 means ANY)
        if (rule.criteria.dst_port != 0 && rule.criteria.dst_port != packet.dst_port)
            continue;

        // 모든 5-Tuple 조건 일치 시 즉시 해당 룰의 정책 반환 (First-Match-Wins)
        return {rule.action, rule.rule_name};
    }

    // 일치하는 규칙이 없을 경우 기본 차단 정책 적용 (Default-Deny policy if no rule matched)
    return {Action::DENY, "Default-Deny"};
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

// L4 TCP/UDP Pseudo-Header struct
// __attribute__((packed)) : struct 안에 padding byte를 넣지 않는다.
struct __attribute__((packed)) pseudo_header {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t reserved;
    uint8_t protocol;
    uint16_t length;
};

// TCP Checksum calculation function (RFC 793)
uint16_t calculate_tcp_checksum(struct iphdr *ip_header, struct tcphdr *tcp_header) {
    uint16_t ip_header_len = ip_header->ihl * 4;
    uint16_t tcp_seg_len = ntohs(ip_header->tot_len) - ip_header_len;

    // 계산을 위한 pseudo header 정보 입력
    pseudo_header psh{};
    psh.src_ip = ip_header->saddr;
    psh.dst_ip = ip_header->daddr;
    psh.reserved = 0;
    psh.protocol = IPPROTO_TCP;
    psh.length = htons(tcp_seg_len);

    char pseudo_packet[2048];
    memcpy(pseudo_packet, &psh, sizeof(psh));

    tcp_header->check = 0; // 계산 전 초기화
    memcpy(pseudo_packet + sizeof(psh), tcp_header, tcp_seg_len);

    return calculate_checksum(pseudo_packet, sizeof(psh) + tcp_seg_len);
}

// UDP Checksum calculation function (RFC 768)
uint16_t calculate_udp_checksum(struct iphdr *ip_header, struct udphdr *udp_header) {
    uint16_t udp_len = ntohs(udp_header->len);

    // 계산을 위한 pseudo header 정보 입력
    pseudo_header psh{};
    psh.src_ip = ip_header->saddr;
    psh.dst_ip = ip_header->daddr;
    psh.reserved = 0;
    psh.protocol = IPPROTO_UDP;
    psh.length = htons(udp_len);

    char pseudo_packet[2048];
    memcpy(pseudo_packet, &psh, sizeof(psh));

    udp_header->check = 0; // 계산 전 초기화
    memcpy(pseudo_packet + sizeof(psh), udp_header, udp_len);

    uint16_t checksum = calculate_checksum(pseudo_packet, sizeof(psh) + udp_len);
    // RFC 768: 계산 결과가 0이면 0xFFFF 반환 (0은 체크섬 미사용을 의미)
    return (checksum == 0) ? 0xFFFF : checksum;
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

    // 3. Network Interface IP 기반 동적 탐색 (Docker 인터페이스 역전 방지)
    struct ifaddrs *ifaddr = nullptr;
    unsigned int ext_ifindex = 0;
    unsigned int dmz_ifindex = 0;
    std::string ext_ifname = "unknown", dmz_ifname = "unknown";

    if (getifaddrs(&ifaddr) != -1) {
        for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
                continue;

            struct sockaddr_in *sa = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(sa->sin_addr), ip, sizeof(ip));

            if (std::string(ip) == "10.10.0.2") {
                ext_ifindex = if_nametoindex(ifa->ifa_name);
                ext_ifname = ifa->ifa_name;
            } else if (std::string(ip) == "10.20.0.2") {
                dmz_ifindex = if_nametoindex(ifa->ifa_name);
                dmz_ifname = ifa->ifa_name;
            }
        }
        freeifaddrs(ifaddr);
    }

    // fallback: 탐색 실패 시 기본 인터페이스 시도
    if (ext_ifindex == 0) {
        ext_ifindex = if_nametoindex("eth1");
        ext_ifname = "eth1(fallback)";
    }
    if (dmz_ifindex == 0) {
        dmz_ifindex = if_nametoindex("eth0");
        dmz_ifname = "eth0(fallback)";
    }

    if (ext_ifindex == 0 || dmz_ifindex == 0) {
        std::cerr << "[!] Failed to find network interfaces (External or DMZ not found)" << std::endl;
        close(ext_sock);
        close(dmz_sock);
        return 1;
    }

    std::cout << "[+] Interface identified - External: " << ext_ifname << " (" << ext_ifindex << "), DMZ: " << dmz_ifname << " (" << dmz_ifindex << ")" << std::endl;

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

    char buffer[2048]; // packet 수신용

    struct in_addr original_client_ip{}; // 원본 Client IP 백업용

    // NPAT를 위한 세션 저장 테이블
    std::unordered_map<uint16_t, NatSession> session_table;

    // ACL 룰 테이블
    std::vector<AclRule> acl_rules;

    // 1. Reverse Proxy web port 허용 규칙
    acl_rules.push_back(AclRule{
        .rule_name = "ALLOW_HTTP_8080", // rule의 이름. 8080으로 들어오는 http를 허용해준다는 뜻
        // TCP Protocol로, 목적지 포트가 8080인 모든 패킷에 대하여
        .criteria = FiveTuple{
            .protocol = IPPROTO_TCP,
            .src_ip = 0,
            .dst_ip = 0,
            .src_port = 0,
            .dst_port = 8080},
        // 허용한다
        .action = Action::ALLOW});
    //

    // 동적 MAC 학습 캐시. docker 최신은 Random LAA 사용 -> 고정 MAC Address 불가
    uint8_t proxy_mac[ETH_ALEN] = {0};
    bool has_proxy_mac = false;
    uint8_t gateway_mac[ETH_ALEN] = {0};
    bool has_gateway_mac = false;

    // Packet 수신, 중계 Event Loop
    while (true) {
        // kernel에 수신 event 대기 요청
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            std::cerr << "[!] poll() error occurred" << std::endl;
            break;
        }

        // ====================================================================
        // [fds[0]: 인바운드 파이프라인] eth0(외부망) ➔ eth1(DMZ) 포워딩
        // ====================================================================
        if (fds[0].revents & POLLIN) {
            struct sockaddr_ll sll{};
            socklen_t sll_len = sizeof(sll);

            // 1. 외부망으로부터 IPv4 패킷 수신 (Receive IPv4 packet from external net)
            ssize_t data_size = recvfrom(ext_sock, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&sll), &sll_len);

            // 1-1. IPv4 헤더 물리적 최소 크기 검증 (Discard runt packet)
            if (data_size < static_cast<ssize_t>(sizeof(struct iphdr)))
                continue;

            // 1-2. 자체 송출 패킷의 루프백 방지 (Prevent loopback of self-transmitted packets)
            if (sll.sll_pkttype == PACKET_OUTGOING)
                continue;

            // 2. 외부 게이트웨이 MAC 동적 학습 (Dynamically learn external gateway MAC)
            if (!has_gateway_mac) {
                std::memcpy(gateway_mac, sll.sll_addr, ETH_ALEN);
                has_gateway_mac = true;
            }

            // 3. L3 IPv4 헤더 매핑 및 IP 문자열 변환 (Map L3 IPv4 header)
            struct iphdr *ip_header = reinterpret_cast<struct iphdr *>(buffer);
            char src_ip[INET_ADDRSTRLEN];
            char dst_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(ip_header->saddr), src_ip, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &(ip_header->daddr), dst_ip, INET_ADDRSTRLEN);

            // 3-1. 라우터 외부 IP 반사 패킷 차단 (Prevent reflection of router external IP)
            struct in_addr router_ext_ip{};
            inet_pton(AF_INET, "10.10.0.2", &router_ext_ip);
            if (ip_header->saddr == router_ext_ip.s_addr)
                continue;

            // 4. L4 전송 계층 포트 번호 추출 (Extract L4 transport layer ports)
            uint16_t client_src_port = 0;
            uint16_t client_dst_port = 0;
            if (ip_header->protocol == IPPROTO_TCP) {
                struct tcphdr *tcp_hdr = reinterpret_cast<struct tcphdr *>(buffer + (ip_header->ihl * 4));

                client_src_port = ntohs(tcp_hdr->source);
                client_dst_port = ntohs(tcp_hdr->dest);
            } else if (ip_header->protocol == IPPROTO_UDP) {
                struct udphdr *udp_hdr = reinterpret_cast<struct udphdr *>(buffer + (ip_header->ihl * 4));

                client_src_port = ntohs(udp_hdr->source);
                client_dst_port = ntohs(udp_hdr->dest);
            }

            // 5. 5-Tuple 구성 및 Stateless ACL 검사 (Construct 5-Tuple & Evaluate ACL)
            FiveTuple incoming_pkt{
                .protocol = ip_header->protocol,
                .src_ip = ip_header->saddr,
                .dst_ip = ip_header->daddr,
                .src_port = client_src_port,
                .dst_port = client_dst_port};

            AclMatchResult acl_res = evaluate_acl(acl_rules, incoming_pkt);
            if (acl_res.action == Action::DENY) {
                std::cout << get_timestamp() << " [ACL DROP] Rule: " << acl_res.rule_name << " | " << src_ip << ":" << client_src_port << " -> " << dst_ip << ":" << client_dst_port << " (Proto: " << static_cast<int>(ip_header->protocol) << ")" << std::endl;
                continue; // 비인가 패킷 폐기 (Discard unauthorized packet)
            }

            // 실시간 인바운드 트래픽 감사 로그 (Real-time inbound audit log)
            std::cout << get_timestamp() << " " << src_ip << " -> " << dst_ip << " (Proto: " << static_cast<int>(ip_header->protocol) << ", Size: " << data_size << " bytes)" << std::endl;

            // 6. Full NAT: DNAT(목적지 변환) 및 SNAT(출발지 변환)
            // 6-1. DNAT: 목적지를 ReverseProxy(10.20.0.3)로 변경 (DNAT to ReverseProxy)
            struct in_addr target_ip{};
            inet_pton(AF_INET, "10.20.0.3", &target_ip);
            ip_header->daddr = target_ip.s_addr;

            // 6-2. SNAT: 출발지를 라우터 DMZ IP(10.20.0.2)로 변경 (SNAT to router DMZ IP)
            original_client_ip.s_addr = ip_header->saddr;

            struct in_addr router_dmz_if_ip{};
            inet_pton(AF_INET, "10.20.0.2", &router_dmz_if_ip);
            ip_header->saddr = router_dmz_if_ip.s_addr;

            // 7. IP Checksum 재계산 (Recalculate IP Checksum)
            ip_header->check = 0;
            ip_header->check = calculate_checksum(ip_header, ip_header->ihl * 4);

            // 8. L4 NAPT 세션 등록 및 L4 Checksum 재계산 (Register NAPT session & Recalculate L4 checksum)
            if (ip_header->protocol == IPPROTO_TCP) {
                struct tcphdr *tcp_header = reinterpret_cast<struct tcphdr *>(buffer + (ip_header->ihl * 4));

                uint16_t client_port = ntohs(tcp_header->source);
                session_table[client_port] = NatSession{original_client_ip, client_port};

                tcp_header->check = calculate_tcp_checksum(ip_header, tcp_header);
            } else if (ip_header->protocol == IPPROTO_UDP) {
                struct udphdr *udp_header = reinterpret_cast<struct udphdr *>(buffer + (ip_header->ihl * 4));

                uint16_t client_port = ntohs(udp_header->source);
                session_table[client_port] = NatSession{original_client_ip, client_port};

                udp_header->check = calculate_udp_checksum(ip_header, udp_header);
            }

            // 9. eth1(DMZ)로 패킷 송출 (Transmit packet via eth1)
            struct sockaddr_ll out_sll{};
            out_sll.sll_family = AF_PACKET;
            out_sll.sll_protocol = htons(ETH_P_IP);
            out_sll.sll_ifindex = dmz_ifindex;
            out_sll.sll_halen = ETH_ALEN;

            // 미학습 상태면 플러딩(0xFF), 학습 완료 시 1:1 유니캐스트 (Flood if unknown, unicast if known)
            if (has_proxy_mac) {
                memcpy(out_sll.sll_addr, proxy_mac, ETH_ALEN);
            } else {
                memset(out_sll.sll_addr, 0xFF, ETH_ALEN);
            }

            sendto(dmz_sock, buffer, data_size, 0, reinterpret_cast<struct sockaddr *>(&out_sll), sizeof(out_sll));
        }

        // ====================================================================
        // [fds[1]: 아웃바운드 파이프라인] eth1(DMZ) ➔ eth0(외부망) 반환
        // ====================================================================
        if (fds[1].revents & POLLIN) {
            struct sockaddr_ll sll{};
            socklen_t sll_len = sizeof(sll);

            // 1. eth1(DMZ)로부터 백엔드 응답 패킷 수신 (Receive backend response from eth1)
            ssize_t data_size = recvfrom(dmz_sock, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&sll), &sll_len);

            // 1-1. IPv4 헤더 물리적 최소 크기 검증 (Discard runt packet)
            if (data_size < static_cast<ssize_t>(sizeof(struct iphdr)))
                continue;

            // 1-2. 자체 송출 패킷의 루프백 방지 (Prevent loopback of self-transmitted packets)
            if (sll.sll_pkttype == PACKET_OUTGOING)
                continue;

            // 2. 리버스 프록시 MAC 동적 학습 (Dynamically learn ReverseProxy MAC)
            if (!has_proxy_mac) {
                memcpy(proxy_mac, sll.sll_addr, ETH_ALEN);
                has_proxy_mac = true;
            }

            // 3. L3 IPv4 헤더 매핑 (Map L3 IPv4 header)
            struct iphdr *ip_header = reinterpret_cast<struct iphdr *>(buffer);

            // 3-1. ReverseProxy(10.20.0.3) 송신자 검증 (Drop if response is not from ReverseProxy)
            struct in_addr expected_proxy_ip{};
            inet_pton(AF_INET, "10.20.0.3", &expected_proxy_ip);
            if (ip_header->saddr != expected_proxy_ip.s_addr)
                continue;

            // 6. Reverse NAT: 출발지 IP를 라우터 외부 IP로 복원 (Restore src IP to router external IP)
            struct in_addr router_ext_ip{};
            inet_pton(AF_INET, "10.10.0.2", &router_ext_ip);
            ip_header->saddr = router_ext_ip.s_addr;

            // 6-1. L4 세션 테이블 조회 및 목적지 IP 복원 (Lookup session table & restore dst IP)
            if (ip_header->protocol == IPPROTO_TCP) {
                struct tcphdr *tcp_header = reinterpret_cast<struct tcphdr *>(buffer + (ip_header->ihl * 4));
                uint16_t reply_port = ntohs(tcp_header->dest);
                auto it = session_table.find(reply_port);

                // 미등록 세션 비인가 패킷 폐기 (Drop unauthorized packet of unregistered session)
                if (it == session_table.end())
                    continue;

                ip_header->daddr = it->second.client_ip.s_addr;
            } else if (ip_header->protocol == IPPROTO_UDP) {
                struct udphdr *udp_header = reinterpret_cast<struct udphdr *>(buffer + (ip_header->ihl * 4));
                uint16_t reply_port = ntohs(udp_header->dest);
                auto it = session_table.find(reply_port);

                // 미등록 세션 비인가 패킷 폐기 (Drop unauthorized packet of unregistered session)
                if (it == session_table.end())
                    continue;

                ip_header->daddr = it->second.client_ip.s_addr;
            }

            // 7. IP Checksum 재계산 (Recalculate IP Checksum)
            ip_header->check = 0;
            ip_header->check = calculate_checksum(ip_header, ip_header->ihl * 4);

            // 8. L4 Checksum 재계산 (Recalculate L4 TCP/UDP checksum)
            if (ip_header->protocol == IPPROTO_TCP) {
                struct tcphdr *tcp_header = reinterpret_cast<struct tcphdr *>(buffer + (ip_header->ihl * 4));

                tcp_header->check = calculate_tcp_checksum(ip_header, tcp_header);
            } else if (ip_header->protocol == IPPROTO_UDP) {
                struct udphdr *udp_header = reinterpret_cast<struct udphdr *>(buffer + (ip_header->ihl * 4));

                udp_header->check = calculate_udp_checksum(ip_header, udp_header);
            }

            // 9. eth0(외부망)로 패킷 송출 (Transmit packet via eth0)
            struct sockaddr_ll out_sll{};
            out_sll.sll_family = AF_PACKET;
            out_sll.sll_protocol = htons(ETH_P_IP);
            out_sll.sll_ifindex = ext_ifindex;
            out_sll.sll_halen = ETH_ALEN;

            // 외부 게이트웨이로 1:1 유니캐스트 송출 (Unicast to external gateway)
            std::memcpy(out_sll.sll_addr, gateway_mac, ETH_ALEN);

            sendto(ext_sock, buffer, data_size, 0, reinterpret_cast<struct sockaddr *>(&out_sll), sizeof(out_sll));
        }
    }

    close(ext_sock);
    close(dmz_sock);

    return 0;
}
