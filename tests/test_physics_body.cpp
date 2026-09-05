// =============================================================================
// tests/test_physics_body.cpp — PhysicsBody 단위 테스트 (4 케이스).
//
// 검증 대상:
//   - SetMass 가 mass / invMass 를 일관되게 갱신.
//   - 0 mass 가 invMass = 0 으로 (Static body 안전 처리).
//   - ApplyForce / ApplyImpulse 가 forceAccum / impulseAccum 에 정확히 누적.
//
// 케이스 + 잡는 잠재 버그:
//   1. body_setmass_inverts          — mass=2 → invMass=0.5.
//                                       버그: 1/m 대신 m 그대로 두는 오타.
//   2. body_zero_mass_is_static       — mass=0 → invMass=0.
//                                       버그: 1/0 으로 inf/NaN 발생.
//   3. body_apply_force_accumulates   — 두 번 호출 결과가 합산.
//                                       버그: = 로 덮어쓰는 오타 (+= 아님).
//   4. body_apply_impulse_accumulates — Impulse 도 동일 동작.
//                                       버그: forceAccum 에 잘못 누적.
// =============================================================================

#include "assert_eq.h"
#include "test_runner.h"
#include "../src/physics/PhysicsBody.h"

REGISTER_TEST(body_setmass_inverts) {
    PhysicsBody b;
    b.SetMass(2.0f);
    CR_NEAR(b.mass,    2.0f, 1e-6f);
    CR_NEAR(b.invMass, 0.5f, 1e-6f);   // 1 / 2 = 0.5.
}

REGISTER_TEST(body_zero_mass_is_static) {
    PhysicsBody b;
    b.SetMass(0.0f);                   // Static / 무한 질량 표현.
    CR_NEAR(b.invMass, 0.0f, 1e-6f);   // 1/0 이 아닌 0 이어야 안전.
}

REGISTER_TEST(body_apply_force_accumulates) {
    PhysicsBody b;
    b.ApplyForce({1.0f, 2.0f});
    b.ApplyForce({3.0f, 4.0f});
    // (1+3, 2+4) = (4, 6) 이 누적되어 있어야 한다 (덮어쓰기 X).
    CR_NEAR(b.forceAccum.x, 4.0f, 1e-6f);
    CR_NEAR(b.forceAccum.y, 6.0f, 1e-6f);
}

REGISTER_TEST(body_apply_impulse_accumulates) {
    PhysicsBody b;
    b.ApplyImpulse({0.5f, -1.0f});
    b.ApplyImpulse({0.5f,  0.5f});
    // (0.5+0.5, -1+0.5) = (1.0, -0.5).
    // forceAccum 으로 잘못 가는 버그도 본 케이스가 잡는다 (impulseAccum
    // 만 검증).
    CR_NEAR(b.impulseAccum.x,  1.0f, 1e-6f);
    CR_NEAR(b.impulseAccum.y, -0.5f, 1e-6f);
}
