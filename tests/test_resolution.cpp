// =============================================================================
// tests/test_resolution.cpp — ResolveContact 단위 테스트 (4 케이스).
//
// 검증 대상:
//   - 위치 보정이 침투를 거의 풀어주는지.
//   - Static (mass=0) 은 위치가 바뀌지 않는지 (분배 식의 invMass=0 안전).
//   - 이미 분리 중이면 속도 변경이 없는지 (vN > 0 분기).
//   - 완전 탄성 + 등질량 정면 충돌 시 부호 swap 패턴이 나오는지.
//
// 케이스 + 잡는 잠재 버그:
//   1. resolve_pushes_apart                — 겹친 두 동질량 body, 보정 후 거의
//                                            분리. 버그: corr 분배 빠짐.
//   2. resolve_static_only_dynamic_moves   — Static 은 그대로, Dynamic 만 이동.
//                                            버그: Static 에 invMass=0 처리
//                                            누락.
//   3. resolve_separating_velocity_skips   — vN > 0 분기 누락 시 분리 중인
//                                            body 도 비물리적 튕김.
//   4. resolve_elastic_bounce              — e=1 등질량 → 속도 부호 swap.
//                                            버그: -(1+e) 의 1+ 누락 (정지).
// =============================================================================

#include "assert_eq.h"
#include "test_runner.h"
#include "../src/physics/Resolution.h"
#include "../src/physics/PhysicsBody.h"
#include "../src/physics/Collision.h"

// 헬퍼: Make AABB body with mass.
static PhysicsBody MakeAABB(float cx, float cy, float w, float h, float mass) {
    PhysicsBody b;
    b.pos   = {cx, cy};
    b.shape = ShapeType::AABB;
    b.half  = {w * 0.5f, h * 0.5f};
    b.SetMass(mass);
    return b;
}

// ---- 1. 두 body 가 침투해 있을 때 위치가 거의 분리되는지 ----
REGISTER_TEST(resolve_pushes_apart) {
    auto a = MakeAABB(0, 0, 10, 10, 1.0f);
    auto b = MakeAABB(8, 0, 10, 10, 1.0f);   // x 침투 2 (overlap = 10 - 8).

    // 보정 전 gap.
    float gapBefore = (b.pos.x - b.half.x) - (a.pos.x + a.half.x);

    Contact c;
    DetectAABB(a, b, c);
    c.a = 0; c.b = 1;                         // BodyId 는 호출자 책임.

    ResolveContact(c, a, b);

    // 한 번 호출 산식:
    //   corr = max(pen - kSlop, 0) / invMassSum * kPercent
    //        = max(2 - 0.01, 0) / 2 * 0.8 = 0.796
    //   총 분리량 = 2 * corr ≈ 1.592 (a/b 가 양쪽으로 0.796씩 이동, 동질량).
    //   잔여 침투 ≈ 2 - 1.592 = 0.408 → gap ≈ -0.408.
    //
    // 한 번 호출만으로 100 % 보정하지 않는 것은 의도된 설계 (kPercent=0.8 +
    // 다중 iter). 본 테스트는 (1) 보정이 실제로 일어났고 (gap 개선), (2)
    // 잔여 침투가 합리적 범위 (≤ 0.5) 임을 확인.
    //
    // 원래 플랜의 임계값 (-0.1) 은 다중 iter 후의 수렴값을 기대한 것이라
    // 단일 호출 테스트와 맞지 않음. 본 테스트는 단일 호출 동작을 검증하므로
    // 임계 -0.5 로 조정 (Plan Revisability 절차의 코드 정합 fix).
    float gap = (b.pos.x - b.half.x) - (a.pos.x + a.half.x);
    CR_ASSERT(gap > gapBefore);    // 보정이 실제로 일어났음.
    CR_ASSERT(gap >= -0.5f);       // 잔여 침투 합리적 범위.
}

// ---- 2. Static (mass=0) 은 안 움직이고 Dynamic 만 이동 ----
REGISTER_TEST(resolve_static_only_dynamic_moves) {
    auto floor = MakeAABB(0, 100, 200, 20, 0.0f);   // Static.
    auto box   = MakeAABB(0,  95,  10, 10, 1.0f);    // Dynamic. floor 위쪽 살짝 침투.

    Contact c;
    DetectAABB(floor, box, c);
    c.a = 0; c.b = 1;

    Vec2 floorBefore = floor.pos;
    ResolveContact(c, floor, box);

    // floor 는 invMass=0 이라 분배 식에서 0 만큼만 이동 → 그대로.
    CR_NEAR(floor.pos.x, floorBefore.x, 1e-5f);
    CR_NEAR(floor.pos.y, floorBefore.y, 1e-5f);
    // box 는 위쪽으로 밀려야 한다 (y 더 작아짐).
    CR_ASSERT(box.pos.y < 95.0f);
}

// ---- 3. 이미 분리 중이면 속도 변경 없음 ----
REGISTER_TEST(resolve_separating_velocity_skips) {
    auto a = MakeAABB(0, 0, 10, 10, 1.0f);
    auto b = MakeAABB(8, 0, 10, 10, 1.0f);
    a.vel = {-5.0f, 0};                      // a 가 왼쪽으로.
    b.vel = { 5.0f, 0};                      // b 가 오른쪽으로 — 이미 분리 중.

    Contact c;
    DetectAABB(a, b, c);
    c.a = 0; c.b = 1;

    Vec2 vaBefore = a.vel;
    Vec2 vbBefore = b.vel;
    ResolveContact(c, a, b);

    // vN > 0 분기에서 return 하므로 속도는 변경 없음 (위치만 보정).
    CR_NEAR(a.vel.x, vaBefore.x, 1e-5f);
    CR_NEAR(b.vel.x, vbBefore.x, 1e-5f);
}

// ---- 4. 완전 탄성 + 등질량 정면 충돌 → 속도 부호 swap ----
REGISTER_TEST(resolve_elastic_bounce) {
    auto a = MakeAABB(0, 0, 10, 10, 1.0f);
    auto b = MakeAABB(8, 0, 10, 10, 1.0f);
    a.vel = { 4.0f, 0};                      // a 가 오른쪽으로 접근.
    b.vel = {-4.0f, 0};                      // b 가 왼쪽으로 접근.
    a.restitution = 1.0f;                    // 완전 탄성.
    b.restitution = 1.0f;

    Contact c;
    DetectAABB(a, b, c);
    c.a = 0; c.b = 1;

    ResolveContact(c, a, b);

    // 등질량 + e=1 정면 충돌은 속도 부호가 뒤집힌다 (a 는 음, b 는 양).
    // 정확한 값까지 검증하지 않는 이유: 위치 보정량이 normal 방향 정밀도에
    // 영향을 줄 수 있어 부호만 검증해 견고성 확보.
    CR_ASSERT(a.vel.x < 0.0f);
    CR_ASSERT(b.vel.x > 0.0f);
}
