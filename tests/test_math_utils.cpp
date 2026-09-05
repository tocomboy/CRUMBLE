// =============================================================================
// tests/test_math_utils.cpp — Vec2 자유 함수, MathUtils, LerpColor 단위 테스트.
//
// 검증 대상:
//   - src/math/Vec2.h 의 10개 자유 함수 (Dot, Cross, PerpLeft/Right, Reflect,
//     Project, DistanceSq, AngleToDir, DirToAngle)
//   - src/math/MathUtils.h 의 Lerp, InverseLerp, Remap, WrapAngle, Ease 5종
//   - src/render/RenderUtils.h 의 LerpColor
//
// 케이스 전략 (15개):
//   1.  dot_free_function        — 자유 함수 Dot == Vec2::Dot 정적 메서드
//   2.  cross_2d                 — 외적 부호 검증 (CCW +, CW -)
//   3.  perp_left_right          — 수직 벡터 방향 확인
//   4.  reflect_horizontal       — 수평 노멀에 대한 반사
//   5.  project_onto_axis        — x 축으로 투영 → y 성분 0
//   6.  distance_sq              — 3-4-5 삼각형으로 거리 제곱 검증
//   7.  angle_to_dir_roundtrip   — AngleToDir → DirToAngle 왕복 오차
//   8.  lerp_basic               — t=0 / 0.5 / 1 경계값 검증
//   9.  inverse_lerp_basic       — Lerp 의 역연산 검증
//   10. remap_range              — [0,100] → [0,1] 재매핑
//   11. wrap_angle_positive      — 3π → π 정규화
//   12. wrap_angle_negative      — -3π → -π 정규화
//   13. wrap_angle_in_range      — 이미 범위 내 각도는 불변
//   14. easing_endpoints         — 5종 이징 모두 f(0)==0, f(1)==1
//   15. lerp_color_midpoint      — 빨강-녹색 중점 보간 채널 확인
//
// 출처:
//   - [8. Numerical Analysis 1.pdf p.2-p.3]
// =============================================================================

#include "assert_eq.h"
#include "test_runner.h"
#include "../src/math/Vec2.h"
#include "../src/math/MathUtils.h"
#include "../src/render/RenderUtils.h"
#include <SDL.h>   // SDL_Color 타입 (SDL 초기화 불필요 — 타입 사용만)

// =============================================================================
// Vec2 자유 함수 테스트
// =============================================================================

// 테스트 1: dot_free_function
//   - 자유 함수 Dot(a, b) 와 Vec2::Dot(a, b) 정적 메서드가 같은 값을 반환해야 함.
//   - 버그: 자유 함수 Dot 내부에서 x/y 를 뒤바꾸면 정적 메서드와 결과 다름.
REGISTER_TEST(dot_free_function) {
    Vec2 a{1.0f, 2.0f}, b{3.0f, 4.0f};
    // 1*3 + 2*4 = 11.
    CR_NEAR(Dot(a, b), 11.0f, 1e-6f);
    // 자유 함수 == 정적 메서드 (하위 호환 검증).
    CR_NEAR(Dot(a, b), Vec2::Dot(a, b), 1e-6f);
}

// 테스트 2: cross_2d
//   - 2D 외적 스칼라: a×b > 0 이면 a 에서 b 로 반시계(CCW).
//   - (1,0)×(0,1) = 1 (CCW), (0,1)×(1,0) = -1 (CW).
//   - 버그: x 와 y 인덱스를 뒤바꾸면 부호 반전.
REGISTER_TEST(cross_2d) {
    Vec2 right{1.0f, 0.0f};
    Vec2 up{0.0f, 1.0f};
    CR_NEAR(Cross(right, up),  1.0f, 1e-6f);   // 반시계 → 양수.
    CR_NEAR(Cross(up, right), -1.0f, 1e-6f);   // 시계   → 음수.
}

// 테스트 3: perp_left_right
//   - PerpLeft((1,0))  == (0,1)  — 반시계 90° 회전.
//   - PerpRight((1,0)) == (0,-1) — 시계 90° 회전.
//   - 버그: 부호 실수로 PerpLeft 가 (0,-1) 을 반환.
REGISTER_TEST(perp_left_right) {
    Vec2 v{1.0f, 0.0f};
    Vec2 pl = PerpLeft(v);
    Vec2 pr = PerpRight(v);
    CR_NEAR(pl.x, 0.0f,  1e-6f);
    CR_NEAR(pl.y, 1.0f,  1e-6f);
    CR_NEAR(pr.x, 0.0f,  1e-6f);
    CR_NEAR(pr.y, -1.0f, 1e-6f);
}

// 테스트 4: reflect_horizontal
//   - 노멀 (0,1) (수평면 위 법선) 에 대한 반사.
//   - 입사: (1,-1) → 반사: (1,1). y 성분만 부호 반전.
//   - 버그: n 을 두 번 사용하지 않거나 2배를 빠뜨리면 결과 오류.
REGISTER_TEST(reflect_horizontal) {
    Vec2 v{1.0f, -1.0f};
    Vec2 n{0.0f,  1.0f};   // 수평면 위 법선 (단위 벡터).
    Vec2 r = Reflect(v, n);
    CR_NEAR(r.x,  1.0f, 1e-6f);
    CR_NEAR(r.y,  1.0f, 1e-6f);
}

// 테스트 5: project_onto_axis
//   - (3,4) 를 x 축 (1,0) 으로 투영 → (3, 0).
//   - y 성분이 0 이어야 함 (x 축 방향 성분만 남음).
//   - 버그: onto·onto 로 정규화 안 하면 (3,0) 대신 (3,0)*onto 가 됨.
REGISTER_TEST(project_onto_axis) {
    Vec2 v{3.0f, 4.0f};
    Vec2 xAxis{1.0f, 0.0f};
    Vec2 p = Project(v, xAxis);
    CR_NEAR(p.x, 3.0f, 1e-6f);
    CR_NEAR(p.y, 0.0f, 1e-6f);
}

// 테스트 6: distance_sq
//   - (0,0)~(3,4) 거리 제곱 = 9+16 = 25.
//   - 버그: LengthSq 대신 Length 를 쓰면 5 를 반환.
REGISTER_TEST(distance_sq) {
    Vec2 origin{0.0f, 0.0f};
    Vec2 point{3.0f, 4.0f};
    CR_NEAR(DistanceSq(origin, point), 25.0f, 1e-6f);
    // Distance 도 함께 검증 (sqrt(25) = 5).
    CR_NEAR(Distance(origin, point),  5.0f,  1e-5f);
}

// 테스트 7: angle_to_dir_roundtrip
//   - AngleToDir(θ) 로 방향 벡터를 만든 뒤 DirToAngle 로 다시 각도를 얻으면
//     원래 θ 와 오차 1e-5f 이내여야 함.
//   - 3개 각도 (0, π/4, -π/2) 로 검증.
//   - 버그: cos/sin 인자 순서나 atan2 y/x 순서가 뒤바뀌면 오차 발생.
REGISTER_TEST(angle_to_dir_roundtrip) {
    float angles[] = {0.0f, kPi / 2.0f, -kPi / 4.0f};
    for (float theta : angles) {
        Vec2  dir   = AngleToDir(theta);
        float back  = DirToAngle(dir);
        CR_NEAR(back, theta, 1e-5f);
    }
}

// =============================================================================
// MathUtils 테스트
// =============================================================================

// 테스트 8: lerp_basic
//   - Lerp(0, 10, 0.5) == 5.
//   - 경계: t=0 → a, t=1 → b.
//   - 버그: (b-a) 대신 (a-b) 를 쓰면 방향 반전.
REGISTER_TEST(lerp_basic) {
    CR_NEAR(Lerp(0.0f, 10.0f, 0.5f), 5.0f,  1e-6f);
    CR_NEAR(Lerp(0.0f, 10.0f, 0.0f), 0.0f,  1e-6f);   // t=0 → a.
    CR_NEAR(Lerp(0.0f, 10.0f, 1.0f), 10.0f, 1e-6f);   // t=1 → b.
}

// 테스트 9: inverse_lerp_basic
//   - InverseLerp(10, 20, 15) == 0.5 — 15 는 [10,20] 의 중점.
//   - 버그: 분모를 (b-a) 대신 (a-b) 로 쓰면 부호 반전.
REGISTER_TEST(inverse_lerp_basic) {
    CR_NEAR(InverseLerp(10.0f, 20.0f, 15.0f), 0.5f, 1e-6f);
    CR_NEAR(InverseLerp(10.0f, 20.0f, 10.0f), 0.0f, 1e-6f);   // 시작점.
    CR_NEAR(InverseLerp(10.0f, 20.0f, 20.0f), 1.0f, 1e-6f);   // 끝점.
}

// 테스트 10: remap_range
//   - Remap(50, 0, 100, 0, 1) == 0.5 — 입력 50% → 출력 0.5.
//   - 버그: inLo/inHi 와 outLo/outHi 순서가 뒤바뀌면 결과 오류.
REGISTER_TEST(remap_range) {
    CR_NEAR(Remap(50.0f, 0.0f, 100.0f, 0.0f, 1.0f), 0.5f, 1e-6f);
    CR_NEAR(Remap( 0.0f, 0.0f, 100.0f, 0.0f, 1.0f), 0.0f, 1e-6f);
    CR_NEAR(Remap(100.0f,0.0f, 100.0f, 0.0f, 1.0f), 1.0f, 1e-6f);
}

// 테스트 11: wrap_angle_positive
//   - WrapAngle(3π) = 3π - 2π = π.
//   - 버그: fmod 의 부호 처리를 누락하면 2π 이상 값이 남음.
REGISTER_TEST(wrap_angle_positive) {
    float wrapped = WrapAngle(3.0f * kPi);
    CR_NEAR(wrapped, kPi, 1e-5f);
}

// 테스트 12: wrap_angle_negative
//   - WrapAngle(-3π) = -3π + 2π = -π.
//   - 버그: 음수 분기 처리가 없으면 -π 대신 π 를 반환.
REGISTER_TEST(wrap_angle_negative) {
    float wrapped = WrapAngle(-3.0f * kPi);
    CR_NEAR(wrapped, -kPi, 1e-5f);
}

// 테스트 13: wrap_angle_in_range
//   - WrapAngle(1.0f) == 1.0f — 이미 [-π, π] 내 각도는 변하지 않아야 함.
//   - 버그: 불필요한 보정으로 값을 변경하면 실패.
REGISTER_TEST(wrap_angle_already_in_range) {
    CR_NEAR(WrapAngle(1.0f),    1.0f,  1e-6f);
    CR_NEAR(WrapAngle(-1.5f),  -1.5f,  1e-6f);
    CR_NEAR(WrapAngle(0.0f),    0.0f,  1e-6f);
}

// 테스트 14: easing_endpoints
//   - 5종 이징 함수 모두 f(0) == 0, f(1) == 1 을 만족해야 함.
//   - 버그: OutBounce 의 n1/d1 계수가 잘못되면 f(1) != 1.
//   - OutElastic 은 진동이 있어 1e-5f eps 사용.
REGISTER_TEST(easing_endpoints) {
    // InQuad
    CR_NEAR(Ease::InQuad(0.0f),    0.0f, 1e-6f);
    CR_NEAR(Ease::InQuad(1.0f),    1.0f, 1e-6f);
    // OutQuad
    CR_NEAR(Ease::OutQuad(0.0f),   0.0f, 1e-6f);
    CR_NEAR(Ease::OutQuad(1.0f),   1.0f, 1e-6f);
    // SmoothStep
    CR_NEAR(Ease::SmoothStep(0.0f),0.0f, 1e-6f);
    CR_NEAR(Ease::SmoothStep(1.0f),1.0f, 1e-6f);
    // OutBounce
    CR_NEAR(Ease::OutBounce(0.0f), 0.0f, 1e-5f);
    CR_NEAR(Ease::OutBounce(1.0f), 1.0f, 1e-5f);
    // OutElastic
    CR_NEAR(Ease::OutElastic(0.0f),0.0f, 1e-5f);
    CR_NEAR(Ease::OutElastic(1.0f),1.0f, 1e-5f);
}

// =============================================================================
// RenderUtils 테스트
// =============================================================================

// 테스트 15: lerp_color_midpoint
//   - LerpColor(빨강, 녹색, 0.5) → r≈127, g≈127, b=0, a=255.
//   - eps 1.5f: Uint8 반올림으로 인해 127 또는 128 이 될 수 있음.
//   - 버그: 채널 순서가 뒤바뀌면 r/g 값 위치 오류.
REGISTER_TEST(lerp_color_midpoint) {
    SDL_Color red   = {255, 0, 0, 255};
    SDL_Color green = {0, 255, 0, 255};
    SDL_Color mid   = LerpColor(red, green, 0.5f);
    // r: 255*0.5 = 127.5 → 127 (truncation).
    CR_NEAR(static_cast<float>(mid.r), 127.0f, 1.5f);
    // g: 0 + 255*0.5 = 127.5 → 127.
    CR_NEAR(static_cast<float>(mid.g), 127.0f, 1.5f);
    // b: 0 (양쪽 모두 0).
    CR_NEAR(static_cast<float>(mid.b),   0.0f, 1e-6f);
    // a: 255 (양쪽 모두 255).
    CR_NEAR(static_cast<float>(mid.a), 255.0f, 1e-6f);
    // t=0 이면 c1 반환 확인.
    SDL_Color atZero = LerpColor(red, green, 0.0f);
    CR_NEAR(static_cast<float>(atZero.r), 255.0f, 1e-6f);
    CR_NEAR(static_cast<float>(atZero.g),   0.0f, 1e-6f);
    // t=1 이면 c2 반환 확인.
    SDL_Color atOne = LerpColor(red, green, 1.0f);
    CR_NEAR(static_cast<float>(atOne.r),  0.0f, 1e-6f);
    CR_NEAR(static_cast<float>(atOne.g), 255.0f, 1e-6f);
}
