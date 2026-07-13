# Bartimaeus App - Security Hardening TODO List

이 파일은 프로젝트의 본래 디자인(투박함)과 실무 기능을 점진적으로 개발해 나가며, 식별된 핵심 보안 위협(Threats)을 차례대로 해결해 나가기 위한 현재/미래의 백로그 관리 목록입니다. 완료된 항목은 제거되며, 현재 진행 중인 항목은 `[/]`로 마킹됩니다.

---

## 📌 보안 강화 작업 목록 (Checklist)

- [ ] **비밀번호 복잡도 유효성 검증 규칙 도입 (THREAT-01)**
    - [ ] `AuthService::signUp` 시 비밀번호 최소 길이 및 구성 조건 정규식 검사 함수 추가 (Add password length and pattern validation in AuthService::signUp)
    - [ ] 검증 실패 시 오류 메시지 프론트엔드로 전달 및 예외 정책 수립 (Pass error messages to frontend and establish policy exceptions)

- [ ] **Docker 기반 인프라 확장 및 MySQL 데이터베이스 마이그레이션 (THREAT-06)**
    - [ ] CMakeLists.txt 내 mysqlclient 라이브러리 연동 (Link mysqlclient library in CMakeLists.txt)
    - [ ] db_queries.hpp 내 SQL 스키마 및 쿼리 MySQL 호환성 튜닝 (Tune SQL schemas and queries for MySQL compatibility in db_queries.hpp)
    - [ ] MySQL Connection Pool (db_manager.hpp) 구현 (Implement MySQL Connection Pool in db_manager.hpp)
    - [ ] AuthService 및 BoardService 내 sqlite3 API를 MySQL C API로 포팅 (Port sqlite3 APIs to MySQL C APIs in AuthService and BoardService)
    - [ ] Docker Compose를 활용한 App 및 MySQL 컨테이너 기반 인프라 구축 (Build App and MySQL container-based infrastructure using Docker Compose)
    - [ ] 멀티 프로세스 로드 밸런싱 환경에서 데이터베이스 동시성 격리 수준 검증 (Verify database concurrency isolation levels in a multi-process load-balanced environment)
