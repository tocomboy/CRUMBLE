// =============================================================================
// tests/test_collision_aabb.cpp — AABB-AABB 검출 단위 테스트 (4 케이스).
//
// 검증 대상:
//   - DetectAABB 가 (1) 분리 시 false, (2) x 축 겹침 시 +x normal, (3) y 축
//     겹침이 더 얕을 때 y 축으로 normal 선택, (4) 정확히 닿은 경계는 분리로
//     처리.
//
// 케이스 + 잡는 잠재 버그:
//   1. aabb_no_overlap_separated      — 떨어져 있을 때 false.
//                                        버그: half 합 비교 부등호 뒤집힘.
//   2. aabb_overlap_x                 — x 축으로 2 만큼 침투, normal=(1,0).
//                                        버그: normal 부호 잘못 (d.x 검사
//                                        반대 방향).
//   3. aabb_overlap_y_picks_min_axis  — x 침투 10, y 침투 2 일 때 y 선택.
//                                        버그: 항상 x 만 보거나, ≤ 비교 잘못.
//   4. aabb_touching_no_overlap       — 정확히 닿은 경계 (overlap=0) 는 false.
//                                        버그: <= 0 조건이 < 0 로 되어 0 을
//                                        true 로 잘못 판정.
// =============================================================================

#include "assert_eq.h"
#include "test_runner.h"
#include "../src/physics/Collision.h"
#include "../src/physics/PhysicsBody.h"

// 헬퍼: 중심 (cx, cy) + 폭/높이 (w, h) 인 AABB body 를 만든다.
//   - 본 헬퍼는 테스트 가독성용. 매 케이스마다 PhysicsBody 7 줄 초기화를
//     반복하지 않도록.
//   - static 으로 internal linkage — 다른 .cpp 와 충돌 없음.
static PhysicsBody MakeAABB(float cx, float cy, float w, float h) {
    PhysicsBody b;
    b.pos   = {cx, cy};
    b.shape = ShapeType::AABB;
    b.half  = {w * 0.5f, h * 0.5f};   // 실제 폭/높이를 half-extents 로 변환.
    return b;
}

// ---- 1. 떨어진 두 박스 — 충돌 없음 ----
REGISTER_TEST(aabb_no_overlap_separated) {
    auto a = MakeAABB(0,  0, 10, 10);   // 중심 (0,0), 좌표 (-5..5, -5..5).
    auto b = MakeAABB(20, 0, 10, 10);   // 중심 (20,0), 좌표 (15..25, -5..5).
    Contact c{};
    CR_ASSERT(!DetectAABB(a, b, c));    // 사이 간격 10 → false.
}

// ---- 2. x 축으로 살짝 겹침 — normal +x ----
REGISTER_TEST(aabb_overlap_x) {
    auto a = MakeAABB(0, 0, 10, 10);    // (-5..5).
    auto b = MakeAABB(8, 0, 10, 10);    // (3..13). x 축으로 2 만큼 겹침.
    Contact c{};
    CR_ASSERT(DetectAABB(a, b, c));
    CR_NEAR(c.penetration, 2.0f, 1e-5f);
    // d.x = 8 (b 가 a 의 오른쪽) → a→b normal = (+1, 0).
    CR_NEAR(c.normal.x, 1.0f, 1e-5f);
    CR_NEAR(c.normal.y, 0.0f, 1e-5f);
}

// ---- 3. y 침투가 더 얕을 때 y 축이 선택되는지 ----
REGISTER_TEST(aabb_overlap_y_picks_min_axis) {
    auto a = MakeAABB(0, 0, 10, 10);
    auto b = MakeAABB(0, 8, 10, 10);    // 동일한 x — y 침투 2, x 침투 10.
    Contact c{};
    CR_ASSERT(DetectAABB(a, b, c));
    CR_NEAR(c.penetration, 2.0f, 1e-5f);
    // 더 얕은 y 축 선택. d.y = 8 > 0 → normal = (0, +1).
    CR_NEAR(c.normal.x, 0.0f, 1e-5f);
    CR_NEAR(c.normal.y, 1.0f, 1e-5f);
}

// ---- 4. 경계가 정확히 닿았을 때 (overlap=0) — 분리로 처리 ----
REGISTER_TEST(aabb_touching_no_overlap) {
    auto a = MakeAABB(0,  0, 10, 10);   // 우측 끝 = +5.
    auto b = MakeAABB(10, 0, 10, 10);   // 좌측 끝 = +5. 정확히 맞닿음.
    Contact c{};
    // overlapX = (5 + 5) - 10 = 0 → strict <=0 검사로 false.
    // 본 검증으로 "<" 가 아닌 "<=" 사용 여부가 보장된다.
    CR_ASSERT(!DetectAABB(a, b, c));
}
