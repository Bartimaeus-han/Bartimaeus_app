#include <iostream>
#include <linux/if_packet.h> // sockaddr_ll struct
#include <net/ethernet.h>    // ETH_P_IP protocol constant
#include <net/if.h>          // if_nametoindex()
#include <netinet/in.h>      // htons()
#include <poll.h>            // poll(), struct pollfd
#include <sys/socket.h>      // socket(), AF_PACKET, SOCK_DGRAM
#include <unistd.h>          // close() system call

int main() {
    // 1. eth0 전용 Raw socket 생성
    int ext_sock = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
    if (ext_sock < 0) {
        // error msg
    }
    // 2. eht1 전용 Raw socket 생성
    int dmz_sock = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
    if (dmz_sock < 0) {
        // error msg
    }

    std::cout << "[+] Sockets created successfully (ext_sock: " << ext_sock << ", dmz_sock: " << dmz_sock << ")" << std::endl;

    // 3. Network Interface index 조회
    unsigned int ext_ifindex = if_nametoindex("eth0");
    unsigned int dmz_ifindex = if_nametoindex("eth1");

    if (ext_ifindex == 0 || dmz_ifindex == 0) {
        // error msg & close() & return
    }

    std::cout << "[+] Interface identified - eth0(External): " << ext_ifindex << ", eth1(DMZ): " << dmz_ifindex << std::endl;

    // 4. 각 socket을 해당하는 network interface에 bind
    struct sockaddr_ll ext_sll{};
    ext_sll.sll_family = AF_PACKET;
    ext_sll.sll_protocol = htons(ETH_P_IP);
    ext_sll.sll_ifindex = ext_ifindex;
    if (bind(ext_sock, reinterpret_cast<struct sockaddr *>(&ext_sll), sizeof(ext_sll)) < 0) {
        // error
    }

    struct sockaddr_ll dmz_sll{};
    dmz_sll.sll_family = AF_PACKET;
    dmz_sll.sll_protocol = htons(ETH_P_IP);
    dmz_sll.sll_ifindex = dmz_ifindex;
    if (bind(dmz_sock, reinterpret_cast<struct sockaddr *>(&dmz_sll), sizeof(dmz_sll)) < 0) {
        // error
    }

    std::cout << "[+] Sockets successfully bound to respective interfaces" << std::endl;

    // 5. I/O multiplexing을 위한 pollfd 구조체 배열 구성
    struct pollfd fds[2];
    fds[0].fd = ext_sock;
    fds[0].events = POLLIN; // eth0 수신 대기

    fds[1].fd = dmz_sock;
    fds[1].events = POLLIN; // eth1 수신 대기

    std::cout << "[*] Screening Router packet forwarding loop started..." << std::endl;

    while (true) {
        // kernel에 수신 event 대기 요청
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            std::cerr << "[!] poll() error occurred" << std::endl;
            break;
        }

        if (fds[0].revents & POLLIN) {
            // forwarding logic
        }

        if (fds[1].revents & POLLIN) {
            // forwarding logic
        }
    }

    close(ext_sock);
    close(dmz_sock);

    return 0;
}
