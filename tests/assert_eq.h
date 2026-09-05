// =============================================================================
// tests/assert_eq.h — 핸드롤 단위 테스트용 assertion 매크로.
//
// 목적:
//   - 외부 테스트 프레임워크 없이 사용할 수 있는 가벼운 검증 도구.
//   - 실패 시 std::runtime_error 를 throw 해 test_main.cpp 의 catch 블록이
//     [FAIL] 메시지로 처리하도록 한다.
//
// 두 가지 매크로 제공:
//   - CR_ASSERT(cond)            : cond 가 false 면 실패. 일반 bool 검증.
//   - CR_NEAR(a, b, eps)         : |a - b| > eps 이면 실패. 부동소수 근사.
//
// "CR_" 접두사는 Crumble 의 약자. 다른 헤더의 매크로와 충돌하지 않도록 짧고
// 식별 가능한 prefix 를 골랐다.
//
// 출처: 메인 플랜 P0 Task 0.4 의 명세. 강의 PDF 에는 테스트 매크로가 다루어
//       지지 않으므로 본 매크로는 프로젝트 자체 표준이다 (PDF 출처 없음).
// =============================================================================

#pragma once

#include <cmath>         // std::fabs — 부동소수 절댓값.
#include <cstdio>        // std::snprintf — 진단 메시지 포맷.
#include <stdexcept>     // std::runtime_error — 실패 시 throw 할 예외 타입.
#include <string>        // (snprintf 결과를 char[] 로 받지만, std::string 보조 용도로 include 유지.)

// CR_ASSERT(cond)
//   - cond 가 false 일 때 std::runtime_error 를 throw.
//   - 메시지에 표현식 텍스트, 파일, 라인 번호를 포함해 디버깅을 돕는다.
//   - do { ... } while(0) 관용은 매크로를 if/else 안에서 안전하게 쓸 수 있게
//     해주는 표준 패턴 — 예: `if (...) CR_ASSERT(x); else ...;` 가 의도대로
//     컴파일되도록.
#define CR_ASSERT(cond) do {                                                   \
    if (!(cond)) {                                                              \
        char buf[256];                                                          \
        std::snprintf(buf, sizeof(buf),                                         \
            "assertion failed: %s (%s:%d)",                                     \
            #cond, __FILE__, __LINE__);                                         \
        throw std::runtime_error(buf);                                          \
    }                                                                           \
} while (0)

// 부동소수 근사 비교 — `a` 와 `b` 의 차이가 `eps` 보다 크면 실패.
//   - 매크로가 아닌 inline 함수로 두는 이유: 인자가 두 번 평가되지 않도록
//     (매크로는 표현식 부작용이 두 번 실행되는 함정이 있다).
//   - file / line 은 호출 측에서 매크로로 채워 넣는다 (아래 CR_NEAR).
inline void cr_assert_near(float a, float b, float eps,
                            const char* file, int line) {
    if (std::fabs(a - b) > eps) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "near failed: %f vs %f (eps=%f) (%s:%d)",
            a, b, eps, file, line);
        throw std::runtime_error(buf);
    }
}

// CR_NEAR(a, b, eps)
//   - cr_assert_near 에 호출 위치 (__FILE__, __LINE__) 를 자동 첨부.
//   - 단위 테스트에서 dt × velocity 같은 부동소수 결과 검증에 사용.
#define CR_NEAR(a, b, eps) cr_assert_near((a), (b), (eps), __FILE__, __LINE__)
