# Project Architecture & Security Patterns

이 문서는 AI가 프로젝트의 전체적인 폴더 구조의 논리적 의미, 적용된 아키텍처 패턴, 그리고 핵심적인 보안 아키텍처 구현 방식을 오해 없이 이해하도록 돕는 장기 메모리 파일입니다.

---

## 1. 디렉토리 구조 및 역할 (Directory Structure)

프로젝트 루트 아래의 각 폴더 및 주요 파일들의 역할 분담은 다음과 같습니다.

* **`src/`**: 백엔드 C++ 소스 코드가 위치하는 디렉토리
  * **[main.cpp](file:///c:/Projects/Bartimaeus_app/src/main.cpp)**: 애플리케이션 진입점. 서버 생성, 라우팅 정의, 전역 시그널 처리 및 기본 보안 HTTP 헤더 탑재를 총괄합니다.
  * **`controllers/`**: HTTP 요청 분석 및 응답 처리 계층
    * [auth_controller.hpp](file:///c:/Projects/Bartimaeus_app/src/controllers/auth_controller.hpp): 로그인, 로그아웃, 회원가입, 사용자 정보 조회 엔드포인트 제어 및 응답 JSON Escaping 처리.
  * **`services/`**: 핵심 비즈니스 로직 및 외부 연동(DB, 세션 관리 등) 계층
    * [auth_service.hpp](file:///c:/Projects/Bartimaeus_app/src/services/auth_service.hpp): 사용자 가입/로그인 로직, SHA256 암호화 처리, DB 쿼리 실행(Prepared Statements 적용).
    * [session_manager.hpp](file:///c:/Projects/Bartimaeus_app/src/services/session_manager.hpp): 난수 기반 세션 ID 발급 및 메모리 기반 화이트리스트 세션 테이블 검증.
  * **`db_queries.hpp`**: SQLite3 쿼리 보조 상수 및 테이블 빌드 함수.
* **`public/`**: 웹 프론트엔드 정적 리소스 (HTML, JS, CSS)
  * [index.html](file:///c:/Projects/Bartimaeus_app/public/index.html) / `admin.html` / `login.html`: 각각의 화면 템플릿.
  * `*.js` 및 `*.css` 정적 스크립트/스타일시트 파일 (보안 CSP 준수를 위해 인라인 코드 배제).
* **`server.db`**: 애플리케이션의 영속성 저장소인 SQLite3 데이터베이스 파일.
* **`CMakeLists.txt`**: C++ 빌드 환경설정 정의.

---

## 2. 디자인 패턴 및 아키텍처 (Design Patterns & Architecture)

* **Layered Architecture (계층형 아키텍처)**:
  * 클라이언트의 HTTP 요청은 **Controller 계층**(`auth_controller`)에서 입력값 유효성 검증과 JSON 출력을 처리합니다.
  * 실제 비즈니스 로직 및 영속성 처리는 **Service 계층**(`auth_service`, `session_manager`)으로 철저하게 격리하여 비즈니스 논리와 통신 프로토콜을 분리하고 있습니다.
* **Graceful Shutdown (우아한 종료)**:
  * [main.cpp](file:///c:/Projects/Bartimaeus_app/src/main.cpp) 내 전역 `httplib::Server` 주소를 매핑하여 `SIGINT` 시그널이 오면 SQLite DB와 소켓을 올바르게 종료하여 데이터 훼손을 방지합니다.

---

## 3. 핵심 보안 구현 패턴 (Key Security Implementation Patterns)

이 프로젝트는 학습 목적에 맞추어 아래의 보안 패턴이 철저히 준수되어 구현되어 있습니다.

### 3.1 SQL Injection 방어
* **구현 방식**: 모든 사용자 입력 변수(`username` 등)는 문자열 접합(Concatenation) 없이 `sqlite3_prepare_v2`를 거친 후 `sqlite3_bind_text` API를 통해 쿼리에 바인딩됩니다 (**Prepared Statements**).
* **목적**: 쿼리의 제어 구조(구문 분석 트리)와 데이터를 사전에 명확하게 구분하여 임의의 악성 페이로드가 SQL 문법으로 해석되는 것을 근본적으로 원천 차단합니다.

### 3.2 세션 및 쿠키 조작 방어
* **구현 방식**: 
  1. 클라이언트 측에 단순 노출되던 회원 ID 쿠키를 폐기하고, UUID 급의 **무작위 난수 세션 ID**를 생성하여 쿠키로 발급합니다.
  2. 서버 측 `SessionManager` 메모리 내에서만 유효 세션 테이블을 관리(White-list 방식)합니다.
  3. 로그인 시 기존 세션을 지우고 새 세션을 할당하여 **Session Fixation** 공격을 방지하며, 세션 쿠키 발급 시 **`HttpOnly`** 및 **`Secure`** 플래그를 설정하여 스크립트 유출 및 평문 전송 경로를 차단합니다.

### 3.3 교차 사이트 스크립팅 (XSS) 방어
* **C++ JSON Escape**: [auth_controller.hpp](file:///c:/Projects/Bartimaeus_app/src/controllers/auth_controller.hpp)에서 JSON 포맷의 문자열 데이터를 리턴할 때, 특수 문자(`<`, `>`, `"`, `\`, `/` 등)를 이스케이프하는 헬퍼를 경유시켜 브라우저 오동작(Stored XSS)을 방어합니다.
* **기본 보안 헤더 및 CSP**:
  * [main.cpp](file:///c:/Projects/Bartimaeus_app/src/main.cpp)에서 `Content-Security-Policy`(`default-src 'self'`)를 포함한 주요 브라우저 보안 헤더들을 응답 기본값으로 강제 설정합니다.
  * 웹 페이지 내의 인라인 스크립트를 허용하지 않고 오직 독립된 `.js` 파일의 코드만 해석하도록 구성하여 반사형/저장형 XSS의 공격면을 통제합니다.
