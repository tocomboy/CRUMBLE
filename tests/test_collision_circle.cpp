// =============================================================================
// test_collision_circle.cpp — Circle-Circle 검출 단위 테스트.
// =============================================================================
//
// 검증 3 가지:
//   1) 분리된 두 원 → false.
//   2) 부분 겹침 → penetration / normal 정확.
//   3) concentric (정확히 같은 위치) → fallback unit vector 반환 + true.
//
// 출처: 메인 플랜 P4 Task 4.1 Step 1.
// =============================================================================

#include "test_runner.h"
#include "assert_eq.h"

#include "../src/physics/Collision.h"
#include "../src/physics/PhysicsBody.h"

namespace {

PhysicsBody MakeCircle(float x, float y, float r) {
    PhysicsBody b;
    b.shape = ShapeType::Circle;
    b.pos   = {x, y};
    b.half  = {r, r};
    return b;
}

}  // namespace

REGISTER_TEST(circle_no_overlap) {
    auto a = MakeCircle(0,  0, 5);
    auto b = MakeCircle(20, 0, 5);
    Contact c{};
    CR_ASSERT(!DetectCircle(a, b, c));
}

REGISTER_TEST(circle_overlap_normal_direction) {
    auto a = MakeCircle(0, 0, 5);
    auto b = MakeCircle(8, 0, 5);   // dist=8, rSum=10 → penetration=2.
    Contact c{};
    CR_ASSERT(DetectCircle(a, b, c));
    CR_NEAR(c.penetration, 2.0f, 1e-4f);
    CR_NEAR(c.normal.x, 1.0f, 1e-4f);
    CR_NEAR(c.normal.y, 0.0f, 1e-4f);
}

REGISTER_TEST(circle_concentric_fallback_normal) {
    auto a = MakeCircle(0, 0, 5);
    auto b = MakeCircle(0, 0, 5);
    Contact c{};
    CR_ASSERT(DetectCircle(a, b, c));
    // fallback unit 벡터 — 길이 1 이면 OK (정확히 (1,0) 이지만 일반화 가능).
    CR_NEAR(c.normal.LengthSq(), 1.0f, 1e-4f);
}
