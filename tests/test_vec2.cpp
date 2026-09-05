// =============================================================================
// tests/test_vec2.cpp — Vec2 단위 테스트 (9 케이스).
//
// 검증 대상:
//   - src/math/Vec2.h 의 산술 / 길이 / 정규화 / 내적.
//
// 케이스 전략 + 잡는 잠재 버그:
//   1. vec2_default_zero          — 기본 생성자가 (0, 0) 을 보장.
//                                    버그: 멤버 기본값이 빠지면 unitialized.
//   2. vec2_add                   — operator+ 좌표별 합.
//                                    버그: 한 좌표만 더하는 오타.
//   3. vec2_sub                   — operator- 좌표별 차.
//   4. vec2_scale                 — operator*(float) 스칼라 곱.
//                                    버그: y 만 곱하거나 부호 잘못.
//   5. vec2_length                — Length, LengthSq.
//                                    버그: 제곱 누락 (절댓값 합 등).
//   6. vec2_normalize_unit        — 정규화 후 단위 벡터.
//   7. vec2_normalize_zero        — 영벡터의 정규화는 (0, 0) (NaN/Inf 방지).
//                                    버그: 임계 1e-9f 를 빼먹어 NaN 발생.
//   8. vec2_dot                   — 내적 산식.
//   9. vec2_compound_add          — operator+= 가 자기 자신을 갱신.
//                                    버그: 체이닝 미지원, 또는 새 객체 반환.
// =============================================================================

#include "assert_eq.h"
#include "test_runner.h"
#include "../src/math/Vec2.h"

REGISTER_TEST(vec2_default_zero) {
    Vec2 v;                         // 기본 생성 — x, y 모두 0 이어야 한다.
    CR_NEAR(v.x, 0.0f, 1e-6f);
    CR_NEAR(v.y, 0.0f, 1e-6f);
}

REGISTER_TEST(vec2_add) {
    Vec2 a{1.0f, 2.0f}, b{3.0f, 4.0f};
    Vec2 c = a + b;                 // (1+3, 2+4) = (4, 6).
    CR_NEAR(c.x, 4.0f, 1e-6f);
    CR_NEAR(c.y, 6.0f, 1e-6f);
}

REGISTER_TEST(vec2_sub) {
    Vec2 a{5.0f, 7.0f}, b{2.0f, 3.0f};
    Vec2 c = a - b;                 // (5-2, 7-3) = (3, 4).
    CR_NEAR(c.x, 3.0f, 1e-6f);
    CR_NEAR(c.y, 4.0f, 1e-6f);
}

REGISTER_TEST(vec2_scale) {
    Vec2 a{2.0f, -3.0f};
    Vec2 c = a * 2.5f;              // (2*2.5, -3*2.5) = (5, -7.5).
    CR_NEAR(c.x, 5.0f, 1e-6f);
    CR_NEAR(c.y, -7.5f, 1e-6f);
}

REGISTER_TEST(vec2_length) {
    Vec2 a{3.0f, 4.0f};             // 3-4-5 직각삼각형.
    CR_NEAR(a.Length(),   5.0f,  1e-5f);   // sqrt(9+16) = 5.
    CR_NEAR(a.LengthSq(), 25.0f, 1e-5f);   // sqrt 호출 없이 25.
}

REGISTER_TEST(vec2_normalize_unit) {
    Vec2 a{0.0f, 7.0f};             // y 축 7 만큼 — 정규화하면 (0, 1).
    Vec2 n = a.Normalized();
    CR_NEAR(n.x, 0.0f, 1e-6f);
    CR_NEAR(n.y, 1.0f, 1e-6f);
}

REGISTER_TEST(vec2_normalize_zero) {
    Vec2 a{0.0f, 0.0f};             // 영벡터.
    Vec2 n = a.Normalized();         // 안전: NaN 이 아니라 (0, 0) 반환.
    CR_NEAR(n.x, 0.0f, 1e-6f);
    CR_NEAR(n.y, 0.0f, 1e-6f);
}

REGISTER_TEST(vec2_dot) {
    Vec2 a{1.0f, 2.0f}, b{3.0f, 4.0f};
    // 1*3 + 2*4 = 11.
    CR_NEAR(Vec2::Dot(a, b), 11.0f, 1e-6f);
}

REGISTER_TEST(vec2_compound_add) {
    Vec2 a{1.0f, 2.0f};
    a += Vec2{4.0f, 5.0f};          // 자기 자신 갱신: (5, 7).
    CR_NEAR(a.x, 5.0f, 1e-6f);
    CR_NEAR(a.y, 7.0f, 1e-6f);
}
