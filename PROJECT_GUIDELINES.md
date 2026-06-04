# 프로젝트 가이드라인 및 메모리 맵 (Project Guidelines & Memory Map)

이 프로젝트는 실무 베스트 프랙티스에 따라 AI가 개발 환경을 정확하게 파악하고 최적의 학습 컨텍스트를 로드할 수 있도록 장기 메모리 및 지침 문서들의 역할을 나누어 배치하였습니다.

아래 가이드 문서들을 참고하여 세션 및 학습을 진행해 주세요.

---

### 1. 🤖 AI 행동 지침 및 상호작용 규칙
* **파일**: [.cursorrules](file:///c:/Projects/Bartimaeus_app/.cursorrules)
* **주요 내용**: 
  * AI가 학습 멘토로서 지켜야 할 6대 규칙 (한 걸음씩 진도 나가기, 파일 직접 쓰기 금지, 주석 영문 병기 등)
  * 사용자의 직접 구현 지향 및 개념 원리 우선 등 선호하는 학습 스타일 정의

### 2. 📝 프로젝트 상황 및 히스토리 (장기 메모리)
* **파일**: [CONTEXT.md](file:///c:/Projects/Bartimaeus_app/CONTEXT.md)
* **주요 내용**:
  * 프로젝트 주요 기술 사양 (C++17, `cpp-httplib`, SQLite3, Port 9090 등)
  * 20개 전체 Git 커밋 내역에 따른 취약점 개선 타임라인 및 개발 히스토리

### 3. 🏗️ 프로젝트 아키텍처 및 보안 설계 패턴
* **파일**: [ARCHITECTURE.md](file:///c:/Projects/Bartimaeus_app/ARCHITECTURE.md)
* **주요 내용**:
  * 폴더 및 주요 소스 파일들의 역할 구분 및 계층형 구조(Controller, Service) 설명
  * SQL Injection 방어(Prepared Statements), 세션 변조 방어(HttpOnly, SessionManager), XSS 방어(JSON Escape, CSP 보안 헤더) 등의 구체적인 방어 설계 구조 기술
