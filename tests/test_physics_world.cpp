// =============================================================================
// tests/test_physics_world.cpp — PhysicsWorld 단위 테스트 (3 케이스).
//
// 검증 대상:
//   - 중력이 Dynamic 만 가속하는지.
//   - Static 이 시간이 흘러도 움직이지 않는지.
//   - 박스가 floor 에 떨어져 안정적으로 안착하는지 (1초간 600 step 후 침투
//     없이 거의 정지).
//
// 케이스 + 잡는 잠재 버그:
//   1. world_gravity_drops_dynamic   — 60 step (1초) 후 vel.y > 100, pos.y > 50.
//                                       버그: gravity 곱셈 순서 잘못, dt 두 번 곱.
//   2. world_static_does_not_move    — 60 step 후 위치 변화 없음.
//                                       버그: Static body 가 적분 분기를 거침.
//   3. world_box_lands_on_floor      — 600 step (10초) 후 (a) floor 위에서 약간
//                                       벗어난 위치, (b) 거의 정지. 버그: 계속
//                                       침투, 또는 떨림이 5px/s 초과.
// =============================================================================

#include "assert_eq.h"
#include "test_runner.h"
#include "../src/physics/PhysicsBody.h"
#include "../src/physics/PhysicsWorld.h"

#include <cmath>     // std::fabs

REGISTER_TEST(world_gravity_drops_dynamic) {
    PhysicsWorld w;
    PhysicsBody def;
    def.pos   = {0, 0};
    def.SetMass(1.0f);
    def.shape = ShapeType::AABB;
    def.half  = {5, 5};
    BodyId id = w.CreateBody(def);

    // 60 step × (1/60) = 1초 시뮬레이션.
    for (int i = 0; i < 60; ++i) {
        w.Step(1.0f / 60.0f);
    }
    PhysicsBody* b = w.GetBody(id);
    CR_ASSERT(b != nullptr);

    // 1초간 중력 980 → 속도 약 980 (마찰 친구는 수직축에 영향 없음).
    // 100 이상이면 명확히 가속됐다고 볼 수 있다.
    CR_ASSERT(b->vel.y > 100.0f);
    // 위치도 유의하게 떨어져 있어야. 1/2 * 980 * 1² ≈ 490 픽셀 — 실제로는
    // semi-implicit Euler 라 약간 더. 50 만 넘으면 통과.
    CR_ASSERT(b->pos.y > 50.0f);
}

REGISTER_TEST(world_static_does_not_move) {
    PhysicsWorld w;
    PhysicsBody def;
    def.pos   = {0, 100};
    def.type  = BodyType::Static;     // Static 으로 둔다.
    def.SetMass(0.0f);                // mass=0 → invMass=0.
    def.shape = ShapeType::AABB;
    def.half  = {100, 5};
    BodyId id = w.CreateBody(def);

    for (int i = 0; i < 60; ++i) {
        w.Step(1.0f / 60.0f);
    }
    PhysicsBody* b = w.GetBody(id);
    CR_ASSERT(b != nullptr);

    // 위치 변경 없어야 한다. 1e-4 는 부동소수 잡음 흡수 마진.
    CR_NEAR(b->pos.y, 100.0f, 1e-4f);
}

REGISTER_TEST(world_box_lands_on_floor) {
    PhysicsWorld w;

    // floor: 너비 400, 높이 100 의 두꺼운 Static AABB.
    //   중심 (0, 200), 윗면 = 200 - 50 = 150.
    //
    // 두께 100 인 이유: P1 단계에는 CCD (Continuous Collision Detection) 가
    // 없어 한 step 이동량이 도형 두께를 초과하면 tunneling 한다.
    // 박스 시작 (0, 50) 에서 floor 까지 95 픽셀, 자유낙하 후 step 당 이동
    // ≈ 7 ~ 12 픽셀 — floor 두께 100 안에 항상 들어와 안전.
    //
    // 게임 실제 맵 (P9) 에서도 빠른 dynamic 이 통과할 위험이 있는 영역은
    // 두꺼운 tile 또는 다층으로 설계. 본 테스트는 알고리즘이 "정상 두께
    // 환경에서 정지" 함을 검증.
    PhysicsBody floorDef;
    floorDef.pos   = {0, 200};
    floorDef.type  = BodyType::Static;
    floorDef.SetMass(0.0f);
    floorDef.shape = ShapeType::AABB;
    floorDef.half  = {200, 50};      // 두께 100.
    w.CreateBody(floorDef);

    // box: 10x10 Dynamic. floor 위 50 픽셀 거리에서 시작.
    //   바닥 = 50 + 5 = 55, floor 윗면 = 150 → 떨어질 거리 95.
    PhysicsBody boxDef;
    boxDef.pos   = {0, 50};
    boxDef.SetMass(1.0f);
    boxDef.shape = ShapeType::AABB;
    boxDef.half  = {5, 5};
    BodyId id = w.CreateBody(boxDef);

    // 10초 (600 step) 시뮬레이션 — 충분한 시간으로 안정화.
    for (int i = 0; i < 600; ++i) {
        w.Step(1.0f / 60.0f);
    }
    PhysicsBody* b = w.GetBody(id);
    CR_ASSERT(b != nullptr);

    // box 가 floor 윗면 (150) 근처에 멈춰 있어야 한다.
    //   - pos.y < 150: floor 안쪽으로 들어가 있지 않음.
    //   - pos.y > 130: 충분히 floor 근처까지 떨어져 있음.
    //   - kSlop=0.01 + kPercent=0.8 + iters=4 의 조합으로 잔여 침투 작음.
    CR_ASSERT(b->pos.y < 150.0f);
    CR_ASSERT(b->pos.y > 130.0f);

    // 거의 정지 — 임계 20 px/s. (world_box_lands_on_floor 본문 계속)
    //
    // 아래 임계 설명은 본 케이스용. 별도 케이스 world_fast_impact_no_overbounce
    // 가 "빠른 충돌 시 과튕김 회귀" 를 따로 검증한다 (ResolveAll 의 위치 보정
    // 분리 fix).
    //
    // 왜 5 가 아닌 20 인가:
    //   매 step 에 중력이 vel.y 에 980/60 ≈ 16.3 px/s 를 더하고, 다음 step 의
    //   충돌 응답이 그것을 0 으로 만든다. 이 micro-oscillation 은 P1 알고리즘
    //   의 정상 동작이다 (sleep mechanism — 작은 속도 body 를 일시 비활성
    //   처리 — 은 P1 범위 밖, P10 폴리싱 시 검토 가능).
    //   임계 20 = 한 step 중력 가속 16.3 + 잡음 마진 3.7. 박스가 floor 에서
    //   "튕겨 올라가는" 비물리적 거동 (vel.y < -10 같은 음수 큰 값) 도
    //   잡아낸다.
    //
    // 시각적으로 발표 시연에서는 stacking 이 매끄러워 보이며, F7 디버그
    // 패널 (P10) 의 frame 시간이 안정 16.67 ms 안에 들어옴.
    CR_ASSERT(std::fabs(b->vel.y) < 20.0f);
}

REGISTER_TEST(world_fast_impact_no_overbounce) {
    // [튕김 버그 회귀 가드] ResolveAll 이 위치 보정을 step 당 1 회로 분리하기
    // 전에는, 빠른 낙하로 한 step 에 깊게 침투하면 같은 penetration 으로 위치
    // 보정이 resolutionIters_(=4) 회 반복돼 약 3.2 배 과보정 → box 가 floor 위로
    // 크게 튕겨 올랐다. 본 테스트는 빠른 충돌 직후 box 가 floor 윗면 위로 과도
    // (>20px) 하게 솟지 않고 안착함을 검증해 그 회귀를 잡는다.
    PhysicsWorld w;

    // floor: 윗면 y = 300 - 50 = 250.
    PhysicsBody floorDef;
    floorDef.pos   = {0, 300};
    floorDef.type  = BodyType::Static;
    floorDef.SetMass(0.0f);
    floorDef.shape = ShapeType::AABB;
    floorDef.half  = {300, 50};
    w.CreateBody(floorDef);

    // box: floor 위 100px 에서 큰 하강 속도로 출발 → 한 step 깊은 침투 유발.
    PhysicsBody boxDef;
    boxDef.pos   = {0, 150};
    boxDef.SetMass(1.0f);
    boxDef.shape = ShapeType::AABB;
    boxDef.half  = {10, 10};
    boxDef.vel   = {0.0f, 900.0f};       // 15 px/step — 충돌 step 에 깊게 침투.
    BodyId id = w.CreateBody(boxDef);

    // 충돌 후 (하강 속도가 소멸한 시점부터) 가장 높이 솟은 지점 (최소 pos.y) 추적.
    bool  impacted    = false;
    float minYAfter   = 1e9f;
    for (int i = 0; i < 240; ++i) {       // 4 초.
        w.Step(1.0f / 60.0f);
        PhysicsBody* b = w.GetBody(id);
        // 첫 충돌 = 큰 하강 속도가 충돌 응답으로 소멸 (vel.y < 50) 한 순간.
        if (!impacted && b->vel.y < 50.0f && b->pos.y > 150.0f) impacted = true;
        if (impacted) minYAfter = std::min(minYAfter, b->pos.y);
    }
    PhysicsBody* b = w.GetBody(id);
    CR_ASSERT(b != nullptr);

    // 핵심 회귀 가드: 충돌 후 box 중심이 floor 윗면(250) 위로 20px 넘게 솟지
    // 않아야 (중심 >= 230). 구버전(과보정)은 ~207 까지 튕겨 올라 실패한다.
    CR_ASSERT(minYAfter > 230.0f);

    // 최종 안착 (floor 윗면 250, box half 10 → 중심 ~240) + 거의 정지.
    CR_ASSERT(b->pos.y < 245.0f && b->pos.y > 235.0f);
    CR_ASSERT(std::fabs(b->vel.y) < 30.0f);
}
