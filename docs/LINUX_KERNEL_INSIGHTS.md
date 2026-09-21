# Linux Kernel Insights & Troubleshooting Guide (LINUX_KERNEL_INSIGHTS.md)

이 문서는 Bartimaeus 프로젝트를 진행하며 리눅스 커널(Linux Kernel)의 네트워크 스택, 소켓 인터페이스, 시스템 자원 관리 밑바닥에서 발생한 장애와 동작 메커니즘을 직접 실증·규명한 기술적 기록입니다. 

단순한 기능 구현을 넘어, KISA 기술직(사이버 침해사고 대응 및 보안진단) 직무기술서와 기술 면접에서 강력한 차별점으로 활용할 수 있는 핵심 커널 인사이트를 체계적으로 정리합니다.

---

## 1. 네트워크 서브시스템 (Network Subsystem)

### 1.1 L2 브로드캐스트 TCP 패킷의 커널 레벨 강제 폐기 (`tcp_v4_rcv`)
* **현상**:
  - `ScreeningRouter`에서 목적지 MAC을 모른다는 이유로 L2 목적지를 브로드캐스트(`FF:FF:FF:FF:FF:FF`)로 지정하여 백엔드(`ReverseProxy`)에 TCP SYN 패킷을 전송했을 때, 백엔드가 패킷을 완전히 무시하고 SYN+ACK 응답을 전혀 회신하지 않음.
* **커널 내부 메커니즘**:
  - 리눅스 커널 소스코드([net/ipv4/tcp_ipv4.c](https://github.com/torvalds/linux/blob/master/net/ipv4/tcp_ipv4.c))의 TCP 수신 처리 함수 `tcp_v4_rcv()`에는 다음과 같은 엄격한 검사 로직이 존재함:
    ```c
    // L2 목적지 MAC이 수신자 인터페이스의 고유 주소(PACKET_HOST)가 아니면 즉시 폐기!
    if (skb->pkt_type != PACKET_HOST)
        goto discard_it;
    ```
  - L2 헤더가 브로드캐스트인 경우 네트워크 드라이버가 패킷 타입을 `PACKET_BROADCAST`로 마킹하므로, L3 IP 헤더가 아무리 정상적인 유니캐스트 IP(`10.20.0.3`)를 가리키고 있더라도 TCP 계층에 도달하기 전에 커널에 의해 조용히 폐기(Silent Discard)됨.
* **해결 및 시사점**:
  - TCP는 본질적으로 1:1 유니캐스트 지향 프로토콜이므로 데이터 패킷을 브로드캐스트로 플러딩(Flooding)하는 방식은 성립할 수 없음.
  - 목적지 MAC을 모를 때는 데이터 패킷 대신 **L2 ARP Request(0x0806)**를 브로드캐스트하여 MAC을 먼저 질의·획득(ARP Probing)하거나, **인바운드 패킷의 출발지 MAC(Src MAC)을 캡처하여 동적으로 학습(Dynamic Learning)**해야 함.
* **📝 KISA 연계 포인트**:
  - RFC 1122 (Host Requirements) 규약: *"호스트는 브로드캐스트 주소로 전달된 TCP 패킷을 반드시 폐기해야 한다."*

---

### 1.2 `AF_PACKET` 원시 소켓과 L4 미바인딩 시 커널의 능동적 `TCP RST` 회신
* **현상**:
  - `ScreeningRouter`가 외부 인터페이스(`eth0`)에서 `AF_PACKET` 원시 소켓을 열고 포트 8080 인바운드 패킷을 정상 수신하여 처리하고 있음에도 불구하고, 외부 클라이언트와의 TCP 핸드셰이크가 강제 종료됨.
* **커널 내부 메커니즘**:
  - `AF_PACKET` 소켓은 디바이스 드라이버 바로 위(L2 직상단)에서 패킷의 사본을 낚아채는 "탭(Tap)" 방식으로 동작함.
  - 패킷의 원본은 여전히 리눅스 커널의 정규 L3/L4 네트워크 스택으로 계속 전달됨.
  - 커널 TCP 스택은 포트 8080에 `bind()`된 정규 L4 소켓이 존재하지 않음을 확인하고, RFC 793 규약에 따라 **"닫힌 포트로 들어온 연결 요청"으로 간주하여 능동적으로 `TCP RST(연결 초기화)` 패킷을 생성해 클라이언트에게 회신**함.
* **해결 및 시사점**:
  - 리눅스 패킷 필터링 프레임워크인 `iptables`를 도입하여, 커널 로컬 스택으로 인입되는 트래픽을 선제적으로 침묵시킴:
    ```bash
    iptables -A INPUT -j DROP
    ```
  - `AF_PACKET`은 `iptables` 규칙보다 앞선 드라이버 레이어에서 패킷을 복제하므로, 사용자 공간의 방화벽 엔진은 패킷을 온전히 수신하면서도 커널 스택의 부작용(TCP RST)을 완벽히 차단함.
* **📝 KISA 연계 포인트**:
  - OSI 7계층별 소켓 인터페이스(`SOCK_STREAM` vs `SOCK_RAW` vs `AF_PACKET`)의 커널 경로 차이 및 TCP 제어 플래그(`RST`) 동작 원리.

---

### 1.3 `AF_PACKET` 송출 패킷의 커널 루프백 현상 (`PACKET_OUTGOING`)
* **현상**:
  - `ScreeningRouter`가 소켓을 통해 패킷을 반대편 인터페이스로 포워딩할 때, 자신이 송출한 패킷이 다시 자기 소켓의 수신 큐로 되돌아와 무한 루프(Infinite Forwarding Loop)를 유발함.
* **커널 내부 메커니즘**:
  - 리눅스 커널은 `AF_PACKET` 소켓으로 패킷을 전송할 때, 네트워크 모니터링 도구(tcpdump 등)가 로컬 송신 패킷까지 캡처할 수 있도록 해당 패킷을 내부 프로미스큐어스 큐에 복제함.
  - 이때 전송된 패킷은 `sockaddr_ll.sll_pkttype` 필드가 `PACKET_OUTGOING`으로 설정된 채로 동일 인터페이스의 수신 소켓에 다시 전달됨.
* **해결 및 시사점**:
  - 패킷 수신 루프 최상단에 패킷 타입 검증 가드를 배치하여, 자신이 송출한 패킷을 즉시 필터링:
    ```cpp
    if (sll.sll_pkttype == PACKET_OUTGOING)
        continue; // 자체 송출 패킷 루프백 폐기
    ```
* **📝 KISA 연계 포인트**:
  - 네트워크 패킷 스니핑 구조 및 리눅스 소켓 필터(`BPF`) 파이프라인.

---

### 1.4 수신 프레임 기반 동적 ARP 캐시 자동 학습
* **현상**:
  - 스크리닝 라우터가 백엔드(`ReverseProxy`)에 브로드캐스트로 패킷을 전송했을 때, 리버스 프록시는 스크리닝 라우터의 MAC 주소를 물어보지 않고도 1:1 유니캐스트로 정확히 응답함.
* **커널 내부 메커니즘**:
  - 리눅스 커널은 네트워크 인터페이스로 프레임이 인입될 때마다, L2 이더넷 헤더의 출발지 MAC(`Src MAC`)과 L3 IP 헤더의 출발지 IP(`Src IP`)를 관찰함.
  - 목적지 주소가 브로드캐스트이더라도 송신자 정보는 유효하므로, 커널은 자신의 ARP 캐시 테이블(`ip neigh`)에 `10.20.0.2 ➔ ROUTER_ETH1_MAC` 매핑을 자동으로 등록(Dynamic Learning)함.
* **해결 및 시사점**:
  - L2 스위치의 CAM 테이블 학습 원리와 동일하게, 수신 프레임의 출발지 정보를 신뢰하고 캐싱하는 커널의 기본 동작을 이해하여 반환 트래픽의 유니캐스트 경로를 보장할 수 있음.
* **📝 KISA 연계 포인트**:
  - ARP 캐시 포이즈닝(ARP Spoofing) 공격이 성립하는 근본 원인(상대방의 일방적인 ARP 통지를 검증 없이 신뢰하고 캐싱하는 커널 특성).

---

## 2. 메모리 및 프로세스 관리 서브시스템 (Memory & Process Subsystem)

### 2.1 `setrlimit(RLIMIT_AS)` 가상 메모리(VSS)와 실제 물리 메모리(RSS)의 괴리
* **현상**:
  - DoS 공격 방어를 위해 `setrlimit(RLIMIT_AS, 128MB)`를 적용했으나, 정상적인 idle 상태에서도 신규 TLS 연결 시 스레드가 무한 대기(Hang)에 빠지며 서버가 마비됨.
* **커널 내부 메커니즘**:
  - `RLIMIT_AS`(Address Space)는 커널이 프로세스에게 할당을 허용하는 **가상 주소 공간(VSS, Virtual Set Size)**의 총량을 제한함.
  - 멀티스레드 C++ 서버는 스레드를 생성할 때마다 스레드 전용 스택 영역으로 기본 8MB~10MB의 가상 주소 공간을 선점(Reserve)함.
  - 실제 물리 메모리(RSS, Resident Set Size)는 수 MB에 불과하더라도, 스레드 풀이 몇 개만 생성되면 가상 주소 공간(VSS) 한도 128MB가 즉시 소진됨.
  - 한도 초과 상태에서 OpenSSL이 TLS 핸드셰이크를 위한 내부 버퍼나 스레드 스택 할당을 요청할 때 `mmap()`/`brk()` 시스템 콜이 `ENOMEM`을 반환하며 커널 단에서 할당이 실패, 스레드가 락을 쥐고 무한 대기함.
* **해결 및 시사점**:
  - 프로세스 가상 주소 공간(VSS)을 제한하는 `RLIMIT_AS`를 방어선에서 제거하고, **리눅스 커널의 `cgroups`(Control Groups) 기반 물리 메모리 제한(`mem_limit`)**으로 전환함.
  - 가상 메모리(VSS)와 물리 메모리(RSS)의 차이를 명확히 이해하고, 시스템 자원 통제는 커널 cgroups 레벨에서 격리해야 안전함을 실증함.
* **📝 KISA 연계 포인트**:
  - OS 가상 메모리 관리(VSS vs RSS vs PSS), `mmap`/`brk` 시스템 콜 메커니즘, 리눅스 cgroups 리소스 격리.
