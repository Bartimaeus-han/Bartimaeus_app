# Project Context & Development History

이 문서는 AI가 프로젝트의 기술 사양 및 그동안의 빌드/보안 패치 변경 이력을 정확히 파악하여, 개발 진행 과정에서 혼동 없이 개발 컨텍스트를 유지할 수 있도록 돕는 장기 메모리 파일입니다.

---

## 1. 프로젝트 기술 사양 (Project Tech Stack)

* **기반 언어**: C++17 이상
* **서버 라이브러리**: `cpp-httplib` (헤더 온리 라이브러리)
* **데이터베이스**: SQLite3 (로컬 파일 기반: `server.db`)
* **빌드 시스템**: CMake (macOS/Unix 및 Windows 크로스 플랫폼 지원 구조)
* **동작 포트**: `9090` 포트 (`0.0.0.0:9090` 리스닝)
* **동작 제어**: `run.ps1` 및 `run.sh` 스크립트를 통한 빌드 및 실행 자동화

---

## 2. Git 커밋 히스토리 및 보안 패치 타임라인 (Git History & Security Timeline)

| 커밋 해시 | 관련 영역 / 파일 | 작업 구분 | 주요 변경 사항 및 보안 방어 맥락 (Security Context) |
| :--- | :--- | :--- | :--- |
| **`a28e24b`** | [main.cpp](file:///c:/Projects/Bartimaeus_app/src/main.cpp) | **Feat (최초 생성)** | `cpp-httplib`를 이용한 웹 서버 기초 뼈대 구축 및 `/signup`, `/login`, `/api/users` 라우트 최초 정의 |
| **`5fb82bc`** | `public/login.html` | **Feat** | 정적 리소스 마운트 폴더 내 로그인 페이지 신설 |
| **`b074924`** | `src/*`, `public/` | **Bug (취약성)** | 회원가입/로그인 기능 및 관리자 페이지 추가. 당시는 패스워드 평문 전송/저장, 사용자 ID 기반 무방비 쿠키 검증 등 대다수 취약점이 방치된 초기 버전 |
| **`d5182f8`** | [main.cpp](file:///c:/Projects/Bartimaeus_app/src/main.cpp) | **Refactor** | `SIGINT(Ctrl+C)` 시그널 처리를 도입하여 안전하게 서버 루프를 중지시키는 **Graceful Shutdown** 구현 |
| **`d6614db`** | `src/db_queries.hpp` | **Feat** | SQLite3 연동 시작 및 애플리케이션 기동 시 `users` 테이블 자동 생성 로직 설계 |
| **`20e6938`** | `src/db_queries.hpp` | **Refactor** | 기존 인메모리 방식 데이터베이스(`user_db`)를 완전히 걷어내고 실물 파일 기반 SQLite3 연동 완성 |
| **`984a0e7`** | `public/index.html` | **Feat** | 대시보드 내 관리자 페이지(admin) 진입 버튼을 로그인 유저 중 `admin` 역할을 가진 계정에만 동적 표시하도록 제어 |
| **`e1fee2b`** | [auth_service.hpp](file:///c:/Projects/Bartimaeus_app/src/services/auth_service.hpp) | **Security (SQLi)** | 사용자 입력값 우회 취약점 방어. `sqlite3_prepare_v2` 및 `sqlite3_bind_text`를 적용해 **Prepared Statements** 구현하여 **SQL Injection 원천 차단** |
| **`4b70cc2`** | `EXCEPTIONS.md` | **Docs** | 예외 처리 표준 및 에러 대응을 위한 가이드 문서 추가 |
| **`727f967`** | `PROJECT_GUIDELINES.md`| **Docs** | AI 개발 협업 가이드라인 최초 작성 |
| **`a081dc4`** | [main.cpp](file:///c:/Projects/Bartimaeus_app/src/main.cpp) | **Security (Crypto)** | 패스워드를 DB 저장/비교 시 **SHA256 해시 암호화** 적용. 로그인 여부에 따라 `/index.html` 또는 `/login.html`로 루트 `/` 분기 및 리다이렉트 구조 도입 |
| **`cf43543`** | [session_manager.hpp](file:///c:/Projects/Bartimaeus_app/src/services/session_manager.hpp) | **Security (Session)** | 단순 쿠키 검증에 의한 권한 탈취 취약점 해결. `SessionManager` 설계 후 난수 토큰 기반의 화이트리스트 세션 검증 도입, 세션 쿠키에 `HttpOnly`를 심어 **Session Hijacking/Fixation 방어** |
| **`bf00409`** | [auth_service.hpp](file:///c:/Projects/Bartimaeus_app/src/services/auth_service.hpp) | **Security (Info)** | `/api/users` 데이터 조회 시 사용자 비밀번호 해시 필드(`password`)가 외부로 흘러나가지 않도록 조회 쿼리 및 구조체 바인딩에서 패스워드 제외 조치 |
| **`8592d76`** | [main.cpp](file:///c:/Projects/Bartimaeus_app/src/main.cpp) | **Chore (Build)** | macOS/Unix 지원을 위해 하드코딩된 Windows 툴체인 경로 제거 및 LF 개행 문자 강제. 서버 실행 기본 포트를 `8080`에서 **`9090`**으로 변경 |
| **`5b122ed`** | [auth_controller.hpp](file:///c:/Projects/Bartimaeus_app/src/controllers/auth_controller.hpp) | **Security (XSS)** | 사용자 입력 정보 및 JSON 응답 데이터 유출 제어를 위해 C++ 레벨에서 **`jsonEscape`** 필터링 적용 (Stored XSS 1차 대응) |
| **`d9ccc64`** | `CMakeLists.txt` | **Chore (Build)** | CMake 빌드 구성 및 크로스플랫폼 타겟 결과물 빌드 구조 최적화 |
| **`7f3e9d2`** | [main.cpp](file:///c:/Projects/Bartimaeus_app/src/main.cpp) | **Security (Headers)** | HTTP 응답 시 **보안 헤더 일괄 탑재** (`Content-Security-Policy`, `X-Content-Type-Options: nosniff`, `X-Frame-Options: DENY`, `X-XSS-Protection: 1; mode=block`) |
| **`5b737d4`** | `public/*.js`, `public/*.html` | **Security (CSP)** | 인라인 스크립트를 완전히 걷어내고 별도 `.js` 정적 파일로 스크립트를 분리하여 **CSP(콘텐츠 보안 정책)와의 호환성 확보 및 XSS 공격 차단** |
| **`4a2b212`** | `src/*` | **Refactor** | 기존에 남아있던 취약한 쿼리 관련 잔재 파일 정리 및 `src/controllers`, `src/services` 형태의 패키지 아키텍처 재정립 |
| **`5e759c5`** | `CMakeLists.txt` | **Refactor** | 크로스 컴파일 호환성 강화를 위해 빌드 시스템 내부 스크립트 수정 및 링کر 설정 보강 |
