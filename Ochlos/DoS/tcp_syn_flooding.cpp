#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>


#ifdef _WIN32
#include <WinSock2.h> // socket handle
#include <ws2tcpip.h> // IP 변환 함수(헤더를 변환해야 하기 때문)
#else
#include <arpa/inet.h> // 엔디안 변환용 (리틀->빅)
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

int main() {
#ifdef _WIN32
    // Windows Socket subsystem 초기화
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    std::cout << "[Ochlos] TCP SYN Flooding Simulation Tool initialized.\n";

    // 공격 대상 서버 정보 설정 (도커 내부에서 돌아가고 있는 컨테이너가 대상)
    const char *target_ip = "127.0.0.1";
    const int target_port = 9090;

    // socket에 대한 구조체를 설정한 후
    struct sockaddr_in target_addr;
    // 메모리 0으로 세팅한 다음
    std::memset(&target_addr, 0, sizeof(target_addr));
    // IPv4 라고 지정해준다 (AF_INET; Address Family InterNET)
    target_addr.sin_family = AF_INET;
    // 그리고 socket의 포트 역시 지정
    // htons; Host TO Network Short (리틀->빅 으로 변환해서 저장)
    target_addr.sin_port = htons(target_port);

    // ip address를 지정해준다. AF_INET을 인자로 넣어줌으로써 IPv4라는 것을 알 수 있다.
    if (inet_pton(AF_INET, target_ip, &target_addr.sin_addr) <= 0) {
        std::cerr << "Invalid target IP address: " << target_ip << "\n";

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }

// Create TCP stream socket: IPv4, TCP
#ifdef _WIN32
    // 통신용 객체인 소켓을 하나 생성하는 과정
    // SOCK_STREAM : socket type을 stream으로. (연결 지향형)
    // IPPROTO_TCP : ip protocol로 TCP를 사용한다.
    // SOCKET : 윈도우에서는 socket handle type을 포인터 크기(unsigned int)로 정의한다.
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    // INVALID_SOCKET은 모든 비트가 1인 값
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

#else
    // Linux에서는 socket까지 File Descripter이다. 그래서 정수 취급
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    // linux에서는 system call 실패 시 -1을 반환하기 때문
    if (sock < 0) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }
#endif

    std::cout << "[+] Socket created successfully. Socket descriptor/handle: " << sock << "\n";

    // 대상 서버로 TCP 3-Way Handshake (SYN) 요청 (Initiate TCP 3-way handshake to target server)
    if (connect(sock, reinterpret_cast<struct sockaddr *>(&target_addr), sizeof(target_addr)) != 0) {
#ifdef _WIN32
        std::cerr << "[-] Connect failed with error: " << WSAGetLastError() << "\n";
        closesocket(sock);
        WSACleanup();
#else
        std::cerr << "[-] Connect failed\n";
        close(sock);
#endif
        return 1;
    }

    std::cout << "[+] Successfully connected to " << target_ip << ":" << target_port << "\n";

#ifdef _WIN32
    // Winsock 전용 닫기 함수
    closesocket(sock);
#else
    // Linux에서는 모든것이 파일이라는 철학이 있기 때문에, sock역시 파일 -> 파일 close system call
    close(sock);
#endif

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}