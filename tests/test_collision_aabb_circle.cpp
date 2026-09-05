// =============================================================================
// test_collision_aabb_circle.cpp — AABB vs Circle 검출 단위 테스트.
// =============================================================================
//
// 검증 3 가지:
//   1) Circle 중심이 AABB 안 — true (penetration > 0).
//   2) Circle 이 AABB 모서리에서 부분 겹침 — true.
//   3) 멀리 떨어진 케이스 — false.
//
// 출처: 메인 플랜 P4 Task 4.2 Step 1.
// =============================================================================

#include "test_runner.h"
#include "assert_eq.h"

#include "../src/physics/Collision.h"
#include "../src/physics/PhysicsBody.h"

namespace {

PhysicsBody MakeAABB(float cx, float cy, float w, float h) {
    PhysicsBody b;
    b.shape = ShapeType::AABB;
    b.pos   = {cx, cy};
    b.half  = {w * 0.5f, h * 0.5f};
    return b;
}

PhysicsBody MakeCircle(float cx, float cy, float r) {
    PhysicsBody b;
    b.shape = ShapeType::Circle;
    b.pos   = {cx, cy};
    b.half  = {r, r};
    return b;
}

}  // namespace

REGISTER_TEST(aabbcircle_circle_inside_aabb) {
    auto box = MakeAABB(0, 0, 20, 20);   // half=(10,10).
    auto cir = MakeCircle(2, 0, 3);       // 원 중심이 box 안.
    Contact c{};
    CR_ASSERT(DetectAABBCircle(box, cir, c));
    // 원 중심이 box 안일 때 closest == circle.pos → diff=(0,0) → fallback
    // (1, 0) normal. penetration = r - 0 = 3.
    CR_NEAR(c.penetration, 3.0f, 1e-4f);
}

REGISTER_TEST(aabbcircle_corner_overlap) {
    auto box = MakeAABB(0, 0, 10, 10);    // half=(5,5).
    auto cir = MakeCircle(7, 7, 4);       // 원 중심 (7,7), 모서리 (5,5) 가
                                          // closest. dist = sqrt(8) ≈ 2.83 < 4.
    Contact c{};
    CR_ASSERT(DetectAABBCircle(box, cir, c));
    // penetration = 4 - sqrt(8) ≈ 1.17.
    CR_NEAR(c.penetration, 4.0f - std::sqrt(8.0f), 1e-4f);
}

REGISTER_TEST(aabbcircle_no_overlap_far) {
    auto box = MakeAABB(0,  0, 10, 10);
    auto cir = MakeCircle(20, 20, 3);
    Contact c{};
    CR_ASSERT(!DetectAABBCircle(box, cir, c));
}
