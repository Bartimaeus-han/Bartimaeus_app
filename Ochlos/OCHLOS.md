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

*(공격 시나리오 실습 진행 시 위 템플릿에 따라 순차적으로 기록됩니다)*

