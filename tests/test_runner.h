// =============================================================================
// tests/test_runner.h — REGISTER_TEST 매크로와 글로벌 레지스트리.
//
// 목적:
//   - 여러 개의 test_*.cpp 파일이 동일한 REGISTER_TEST 매크로를 사용해 같은
//     레지스트리에 등록하기 위해 매크로 / 구조체 / 레지스트리를 공유 헤더로
//     분리한다.
//   - tests/test_main.cpp 의 main() 만이 이 레지스트리를 순회 실행한다.
//
// 분리 이유 (root-cause 노트):
//   - 메인 플랜의 P0 Task 0.4 초안은 매크로를 test_main.cpp 안에 두었으나,
//     이 경우 다른 .cpp 파일에서는 매크로가 보이지 않아 컴파일 실패가 난다.
//     실제 빌드 실행 시 발견되어 본 헤더로 분리. 플랜 본문은 Plan
//     Revisability Policy 절차에 따라 후속 갱신 (post-tag refactor 가 아닌
//     본 phase 가 아직 진행 중이므로 직접 갱신 가능).
//
// ODR 안전:
//   - registry() 는 함수-로컬 static (Meyer's singleton) → 어떤 .cpp 에서
//     호출되든 같은 vector 인스턴스를 반환. 정적 초기화 순서 미정의 사고
//     없음.
//   - Registrar 의 정적 인스턴스 (`reg_<NAME>`) 는 각 .cpp 의 internal
//     linkage 영역에 존재해 이름 충돌 없이 자동 등록.
//   - 함수 정의는 inline 또는 static 으로 두어 multiple definition error 회피.
//
// 출처: 강의 PDF 에 테스트 인프라 자체는 다루지 않으므로 본 패턴은
//       프로젝트 자체 표준 (PDF 출처 없음).
// =============================================================================

#pragma once

#include <functional>    // std::function
#include <string>        // std::string
#include <utility>       // std::move
#include <vector>        // std::vector

// 한 개의 등록된 테스트.
//   - name: REGISTER_TEST 의 인자 (테스트 이름 문자열).
//   - fn  : 본문 — assertion 실패 시 std::runtime_error throw 한다.
struct TestEntry {
    std::string                 name;
    std::function<void()>       fn;
};

// 글로벌 레지스트리 — 함수 안의 static 으로 두어 정적 초기화 순서 미정의
// (static initialization order fiasco) 를 회피.
//   - inline: 헤더에 정의해도 ODR 위반 없도록.
//   - 모든 호출자가 동일한 vector 인스턴스를 보게 된다.
inline std::vector<TestEntry>& registry() {
    static std::vector<TestEntry> v;
    return v;
}

// REGISTER_TEST 매크로의 백킹 객체. 생성자에서 레지스트리에 push.
//   - 매크로가 사용될 때마다 각 .cpp 의 익명 영역에 정적 인스턴스가 생성
//     되며 main() 진입 전에 자동 등록을 마친다.
struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

// REGISTER_TEST(NAME) { ... } 사용법.
//   - 첫째 줄  : 함수 forward declaration.
//   - 둘째 줄  : 정적 Registrar 변수 정의 — 이때 자동 등록.
//   - 셋째 줄  : 본문 정의 시작 (사용자 코드의 { ... } 가 이어진다).
#define REGISTER_TEST(NAME)                                                  \
    static void NAME();                                                       \
    static Registrar reg_##NAME(#NAME, NAME);                                 \
    static void NAME()
