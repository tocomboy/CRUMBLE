// =============================================================================
// tests/test_main.cpp — 테스트 러너의 main 함수.
//
// 목적:
//   - test_runner.h 가 제공하는 글로벌 registry() 를 순회하며 각 테스트를
//     실행하고 [PASS] / [FAIL] 메시지를 출력. 종료 코드는 실패 0개면 0,
//     하나라도 있으면 1.
//
// 매크로 / 등록 인프라는 tests/test_runner.h 로 분리되어 있어 여러 개의
// test_*.cpp 가 동일한 REGISTER_TEST 를 공유한다.
//
// 출처: 강의 PDF 에 다루지 않는 프로젝트 자체 표준 (PDF 출처 없음).
// =============================================================================

#include <cstdio>
#include <exception>

#include "test_runner.h"

int main() {
    int passed = 0;
    int failed = 0;

    // 글로벌 레지스트리에 등록된 모든 테스트를 순서대로 실행.
    // 등록 순서는 정적 초기화 순서에 의존하지만, REGISTER_TEST 가 호출되는
    // .cpp 파일 단위 / 줄 순서대로 push 되므로 사람이 읽기에 자연스럽다.
    for (auto& t : registry()) {
        try {
            t.fn();   // assertion 매크로 실패 시 std::runtime_error throw.
            std::printf("[PASS] %s\n", t.name.c_str());
            ++passed;
        }
        catch (const std::exception& e) {
            // CR_ASSERT / CR_NEAR 가 던진 std::runtime_error 가 잡힌다.
            std::printf("[FAIL] %s: %s\n", t.name.c_str(), e.what());
            ++failed;
        }
        catch (...) {
            // 비-std 예외도 알 수 없는 실패로 분류.
            std::printf("[FAIL] %s: unknown error\n", t.name.c_str());
            ++failed;
        }
    }

    std::printf("---\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
