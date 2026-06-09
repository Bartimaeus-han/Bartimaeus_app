# Project Context & Development History

이 문서는 AI가 프로젝트의 기술 사양 및 그동안의 빌드/보안 패치 변경 이력을 정확히 파악하여, 개발 진행 과정에서 혼동 없이 개발 컨텍스트를 유지할 수 있도록 돕는 장기 메모리 파일입니다.

---

## 1. 프로젝트 기술 사양 (Project Tech Stack)

* **기반 언어**: C++20 이상 (MSVC 18 최신 표준 라이브러리 지원 및 clangd 파싱 호환성을 위해 C++20으로 상향)
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
| **`5e759c5`** | `CMakeLists.txt` | **Refactor** | 크로스 컴파일 호환성 강화를 위해 빌드 시스템 내부 스크립트 수정 및 링커 설정 보강 |
| **Local Changes** | `src/*`, `CMakeLists.txt` | **Security (RateLimit)** | 자동화 무차별 대입 공격(Brute Force) 방어를 위해 ID 기반의 로그인 시도 차단 정책(`LoginLimiter`) 도입 및 `AuthController` / `main.cpp` 연동 |
| **Local Changes** | `src/*`, `public/js/*` | **Security (CSRF)** | 동적 세션 토큰 방식의 Anti-CSRF 방어를 도입하여 세션 정보 반환(`/api/me`) 시 토큰을 발급하고, 로그아웃(`/logout`) POST 요청 시 헤더 검증 수행 |
| **Local Changes** | `src/*`, `public/error.html` | **Security (ErrorPage)** | 상세 에러 정보 유출 방지를 위한 무작위 Error Tracking ID 매핑, 외부 HTML 템플릿 연동, API(/api/*) 경로 JSON 응답 및 이중 로그(콘솔/파일) 기록 적용 |


---

## 3. CMake & 빌드 환경 트러블슈팅 히스토리 (C++20, Ninja, MSVC 연동)

### 3.1 AI 실책 및 교훈 (Mistakes & Lessons Learned)
향후 개발 세션 진행 시 동일한 혼선과 시간 낭비를 방지하기 위해 AI가 저지른 판단 오류와 교훈을 명확히 기록합니다.

1. **에디터 설정 선제 검증 누락**:
   * 현상: 사용자가 에디터 내에서 `CMake: Select a Kit` 명령어 자체가 아예 뜨지 않는다고 호소함.
   * 원인: `.vscode/settings.json` 파일에 `"cmake.useCMakePresets": "always"`로 설정되어 프리셋 모드가 강제 구동 중이었던 점을 사전에 탐색하지 않아 불필요한 해결책을 반복 제시함.
2. **Ninja 제너레이터의 아키텍처 지원 오해**:
   * 현상: `CMakePresets.json`에 무턱대고 `"architecture": "x64"`를 기입했다가 `Generator Ninja does not support platform specification` 에러를 유발함.
   * 원인: 이 옵션은 Visual Studio 제너레이터 전용 옵션이며, Ninja 빌드 도구는 직접 아키텍처 명시를 해석하지 못하고 환경 변수에 전적으로 의존한다는 기본 제약을 망각함.
3. **캐시 오염 파악 지연**:
   * 현상: 설정 파일 롤백 후 빌드를 돌렸으나 계속 컴파일러를 찾지 못하는 동일 에러 반복 발생.
   * 원인: `build_win/CMakeCache.txt`에 기록된 오염된 이전 빌드 설정을 강제로 날리지 않으면 동일한 값으로 계속 시도된다는 점을 간과하여 한 템포 늦게 캐시 클린 명령(`CMake: Delete Cache and Reconfigure`)을 제공함.
4. **UI 화면 정보 환각 (Hallucination)**:
   * 현상: 스크린샷 내 깃 동기화 화살표(`↻`)를 CMake 버튼으로 우기고, 있지도 않은 `[No Active Target]` 텍스트가 상태바에 표시되어 있다고 강변함.
   * 원인: 에디터의 최신 상태바 디자인 기본값 및 Git 플러그인 레이아웃을 확실히 확인하지 않고 지레짐작하여 설명하여 사용자에게 극심한 혼란을 안김.
5. **clangd MSVC 드라이버 모드 인자 해석 및 헤더 누출 에러**:
   * 현상: `.cpp` 파일에서만 C++ 표준 라이브러리(`std::ofstream` 등)에 대한 자동완성 드롭다운이 작동하지 않고 `0 results from Sema`가 반환됨.
   * 원인: 
     * **표준 버전 불일치**: MSVC 18.0 (v19.50)의 최신 표준 라이브러리 헤더들은 C++20 기반으로 작성되어 있어, clangd가 C++17 규격으로 해석할 때 파싱 오류(`incomplete due to errors`)가 누적되어 표준 라이브러리 인덱스가 대거 유실됨.
     * **인자 해석 오류**: `.clangd` 파일에서 `-isystem` 플래그와 경로명을 줄바꿈하여 리스트 요소로 분리해 적었더니, MSVC 드라이버 모드(`--driver-mode=cl`)로 실행 중이던 clangd가 경로명 문자열을 플래그의 파라미터가 아니라 **"빌드 대상 C++ 소스 파일"**로 잘못 인식하여 폴더 자체를 빌드 타겟으로 삼아 구문 분석에 실패함.
   * 해결책:
     * `CMake` 표준 및 `.clangd` 표준을 모두 C++20으로 상향 일치시킴.
     * `.clangd` 파일의 인클루드 경로 지정을 공백이나 줄바꿈 없이 `-imsvc<경로>` 및 `-I<경로>` 형식의 단일 문자열 토큰으로 작성하여 전달함.

### 3.2 현재 프로젝트 빌드 설정 값
* **빌드 제너레이터**: `Ninja` (최종 타겟 실행 파일: `SecureWebServer.exe`)
  * `CMAKE_EXPORT_COMPILE_COMMANDS`를 통해 `compile_commands.json`을 강제 활성화하여 `clangd` 언어 서버가 정상 동작하도록 보장함.
* **컴파일러**: `MSVC 19.50` (64비트 x64) - C++20 표준 활성화
* **IDE (Antigravity IDE) 연동 방식**:
  * `.vscode/settings.json` 내 `"cmake.useCMakePresets"`는 `"never"`로 비활성화하여 **Kit 모드**를 활성화.
  * `"cmake.generator"`는 `"Ninja"`로 고정.
  * `CMake: Select a Kit` 메뉴에서 **`Visual Studio Community Release - amd64`**를 수동 선택하여 빌드 환경을 구축함.
* **명령줄(Terminal)에서의 직접 빌드**:
  * `x64 Native Tools Command Prompt for VS` terminal을 열고 `cmake --preset windows-default`를 통해 64비트 MSVC를 타겟으로 1회 구성한 뒤, 이후에는 일반 terminal에서도 `cmake --build build_win` 명령어로 빌드 가능.

