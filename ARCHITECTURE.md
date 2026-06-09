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
    * [login_limiter.hpp](file:///c:/Projects/Bartimaeus_app/src/services/login_limiter.hpp): 로그인 실패 횟수 누적 및 임시 계정 잠금 정책 관리.
  * **`db_queries.hpp`**: SQLite3 쿼리 보조 상수 및 테이블 빌드 함수.
* **`public/`**: 웹 프론트엔드 정적 리소스 (HTML, JS, CSS)
  * [index.html](file:///c:/Projects/Bartimaeus_app/public/index.html) / `admin.html` / `login.html`: 각각의 화면 템플릿.
  * [error.html](file:///c:/Projects/Bartimaeus_app/public/error.html): 플레이스홀더 방식의 공용 에러 페이지 템플릿 (상세 정보 노출 차단).
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

### 3.4 자동화 공격 (무차별 대입) 방어
* **구현 방식**: `LoginLimiter` 클래스를 이용해 각 `username`별로 로그인 실패 횟수 및 잠금 만료 시각을 메모리 상에서 관리합니다. `AuthController`는 실제 DB 로그인 쿼리를 호출하기 전, `getRemainingLockoutTime` 함수를 사용하여 선제적으로 차단(Pre-Check)을 처리(HTTP Status `429` 반환)합니다.
* **목적**: 무차별적인 무차별 대입(Brute Force) 공격을 차단해 유저 계정을 지키고, 불필요한 DB 해시 검증 연산을 차단하여 서버 가용성(DoS 방지)을 확보합니다.

### 3.5 크로스사이트 요청 위조 (CSRF) 방어
* **구현 방식**:
  1. 유저 로그인 시 서버가 세션 ID와 함께 암호학적으로 강력한 난수 토큰(`csrf_token`)을 생성하여 서버 측 세션 저장소(`SessionManager`)에 바인딩합니다.
  2. 세션 정보 조회 API(`/api/me`) 응답을 통해 토큰을 안전하게 프론트엔드로 전달합니다.
  3. 프론트엔드 자바스크립트는 이 값을 메모리 변수에 보관하고, 상태 변경 POST 요청(`/logout`)을 보낼 때 HTTP 요청 헤더 `X-CSRF-Token`에 담아 보냅니다.
  4. 서버의 `AuthController`는 요청을 처리하기 전 세션에 저장된 토큰 값과 헤더로 전송된 토큰 값을 대조하여 일치하는 경우에만 처리를 허가합니다.
* **목적**: 타 도메인에서 브라우저의 자동 쿠키 전송 동작을 악용해 강제로 발생시키는 상태 변경 요청(세션 파괴, 로그아웃 등)을 무력화하여 요청의 정당한 출처와 유저의 명확한 의도를 강제로 확인합니다.

### 3.6 안전한 커스텀 에러 페이지 및 정보 유출 방어
* **구현 방식**:
  1. **정보 은닉 (Information Concealment)**: 서버 내부의 스택 트레이스나 구체적인 에러 메시지(예: 쿼리 구문 오류, 파일 경로 등)를 사용자 브라우저에 그대로 노출시키지 않습니다.
  2. **에러 추적 ID (Correlation ID)**: 서버 에러 발생 시 고유한 무작위 난수 코드(`ERR-xxxxxx` 형태)를 생성하여, 상세 로그는 서버 측 디스크(`error.log`) 및 콘솔(`std::cerr`)에 해당 ID와 함께 안전하게 기록(Dual Logging)하고, 사용자에게는 이 ID 값과 함께 최소한의 안내 메시지만을 표시합니다.
  3. **하이브리드 분기**: 클라이언트 요청 경로가 API(`/api/*`)인지 일반 정적 페이지인지 판별하여, API의 경우 표준화된 JSON 형식으로 에러 응답을 반환하고, 일반 웹 페이지의 경우 `public/error.html` 템플릿의 플레이스홀더(`{{TITLE}}`, `{{DESCRIPTION}}`, `{{TRACKING_ID}}`)를 C++단에서 치환하여 반환합니다.
* **목적**: 공격자가 에러 메시지 분석을 통해 시스템 내부 정보(OS, 웹 서버 세부 버전, DB 구조 등)를 유추(Information Leakage)하는 공격을 차단하면서도, 내부 관리자가 운영 중 발생한 문제를 발급된 Tracking ID를 통해 디버깅할 수 있는 추적성을 완벽하게 구축합니다.

---

## 4. 향후 보안 및 아키텍처 개선 과제 (Future Improvements)

* **설정 기반 보안 인터셉터 (Configuration-based Security Interceptor)**:
  * 현재 `main.cpp`에서 개별 경로별로 작성된 인증 분기를, Enum(`AuthLevel`) 및 정책 맵(`PAGE_AUTH_POLICIES`) 기반의 일관된 미들웨어 인터셉터 구조로 리팩토링할 예정 (정적 파일 및 API 예외 처리 기능 포함).


