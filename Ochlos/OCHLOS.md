# Ochlos (오클로스) - Offensive Security & Attack Simulation Log

**Ochlos**는 Bartimaeus 프로젝트의 보안 위협을 실증하고 방어 기제(Hardening)를 검증하기 위한 **공격자 C2 / 레드팀 시뮬레이션 환경**입니다.

정보보안기사 국가기술자격 및 KISA 주요정보통신기반시설 기술적 취약점 분석·평가 기준을 바탕으로, OSI 7계층별 공격 시나리오를 직접 코드로 구현하고 테스트베드에 실증합니다.

---

## 🎯 레드팀 운영 원칙 (Red Team Principles)

1. **공격 선행 학습 (Offensive-First)**: 방어 코드를 작성하기 전, 공격자 입장에서 취약점을 실제로 격파·악용하는 스크립트를 작성하여 테스트베드에 실증합니다.
2. **OSI 7계층 명시**: 모든 공격 시나리오는 대상이 되는 OSI 계층(L3 Network, L4 Transport, L7 Application 등)을 명시하여 방어선과 대조합니다.
3. **실증적 계측 (Empirical Diagnostics)**: 공격 결과는 단순 추정이 아닌 시스템 리소스(RSS 메모리, 소켓 상태, CPU, 응답 레이턴시)의 실측치로 검증합니다.
4. **블루팀 피드백 연계**: 공격 실증 후 도출된 취약점과 한계를 블루팀(`Bartimaeus`)에 전달하여 방어 아키텍처 수립에 반영합니다.

---

## 📌 공격 시나리오 템플릿 (Scenario Template)

```markdown
### [SCENARIO-XX] 시나리오 제목
- **공격 목표 (Attack Objective)**: 공격 대상 서비스 / 취약점 식별 영역 (예: L4 소켓 자원, L7 인증 엔드포인트 등)
- **OSI 계층 (Target Layer)**: Layer X (Network / Transport / Application 등)
- **공격 메커니즘 (Mechanics)**: 공격자가 악용하는 시스템/프로토콜/코드 결함 원리
- **실행 스크립트 (Script)**: `파일명.py`
- **테스트베드 실측 결과 (Empirical Results)**:
  - 공격 전/중/후 시스템 상태 (메모리, CPU, 소켓 수, HTTP 상태코드 등)
- **블루팀(Bartimaeus) 방어 권고사항 (Blue Team Feedback)**:
  - 필요한 방어선 (방화벽 룰, 커널 튜닝, L7 Rate Limiter, 세션 회수 정책 등)
```

---

## 📌 공격 실증 이력 (Attack Simulation Log)

### [SCENARIO-01] L4 TCP Connection Starvation & Screening Router 실시간 패킷 탐지
- **공격 목표 (Attack Objective)**: 백엔드 웹 서버의 TCP 수신 대기열 및 워커 스레드 자원 점유 (DoS)
- **OSI 계층 (Target Layer)**: Layer 4 (Transport Layer)
- **공격 메커니즘 (Mechanics)**: 3-Way Handshake를 완료한 후 `closesocket()`(RST/FIN)을 보내지 않고 소켓을 배열에 보관한 채 대기하여 서버의 연결 풀을 고갈시킴.
- **실행 도구 (Tool)**: [Ochlos/DoS/tcp_syn_flooding.cpp](DoS/tcp_syn_flooding.cpp) (C++20 Winsock/POSIX)
- **테스트베드 실측 결과 (Empirical Results)**:
  - `ScreeningRouter.exe` (Port 8080) 기동 후 Ochlos 50개 커넥션 인입 시 실시간 L3 IP (`127.0.0.1`) 및 L4 출발지 Port(`5xxxx`) 감지 로그 50건 연속 출력 확인.
- **블루팀(Bartimaeus) 방어 권고사항 (Blue Team Feedback)**:
  - L3/L4 경계선에서 패킷 가시성을 확보하였으므로, 다음 단계로 검증된 트래픽을 내부 백엔드(`bartimaeus-app:9090`)로 포워딩(중계)하는 파이프라인 및 Stateless IP/Port 룰 필터링 구현 필요.

