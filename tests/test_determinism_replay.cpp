// =============================================================================
// test_determinism_replay.cpp — 결정론 회귀 감지기 (디버깅 도구).
// =============================================================================
//
// 목적:
//   같은 입력 시퀀스를 두 PhysicsWorld 에 적용하면 결과 (pos / vel) 가 byte
//   단위로 동일해야 한다. 결정론은 본 프로젝트의 비협상 불변식 — 한 번 깨지면
//   디버깅 / replay / 발표 시연 모두 영향.
//
// 본 테스트는 발표 시연용 기능이 아니라 **회귀 감지기**:
//   - PhysicsWorld 의 적분 순서 / 응답 산식 / NaN 처리 등이 변경될 때 결정론
//     이 깨졌는지 즉시 알려준다.
//   - 600 frame (10 초) 동안의 같은 입력 → memcmp 로 byte 비교.
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 4 Task 4.3.
//   - 결정론 = "결정론 보호" 비협상 불변식 (CLAUDE.md, plan Performance Budget
//     섹션).
//
// 결정론 측면 노트:
//   - 본 테스트가 byte-equal 을 요구하므로 부동소수 환경 변경 (예: -ffast-math)
//     이 들어오면 즉시 실패. CMakeLists.txt 가 -ffast-math 를 금지하는 이유.
//   - PhysicsWorld 의 ApplyImpulse / Step / DetectContactsNaive / ResolveAll
//     이 모두 결정론적 (랜덤 / 시간 의존 / 외부 상태 의존 없음) 임을 가정.
// =============================================================================

#include "test_runner.h"
#include "assert_eq.h"

#include "../src/physics/PhysicsBody.h"
#include "../src/physics/PhysicsWorld.h"

#include <cstring>     // std::memcmp — POD struct 의 byte 비교.
#include <vector>

REGISTER_TEST(replay_identical_inputs_produce_identical_state) {
    // setup: floor (Static AABB) + dynamic box. id=0 floor, id=1 box.
    auto setup = [](PhysicsWorld& w) {
        PhysicsBody floor;
        floor.type  = BodyType::Static;
        floor.SetMass(0.0f);
        floor.shape = ShapeType::AABB;
        floor.pos   = {0.0f, 200.0f};
        floor.half  = {1000.0f, 5.0f};
        w.CreateBody(floor);

        PhysicsBody box;
        box.type  = BodyType::Dynamic;
        box.SetMass(1.0f);
        box.shape = ShapeType::AABB;
        box.pos   = {0.0f, 0.0f};
        box.half  = {5.0f, 5.0f};
        w.CreateBody(box);
    };

    PhysicsWorld w1, w2;
    setup(w1);
    setup(w2);

    // 5 frame 동안의 impulse 시퀀스. 그 후 frame 은 0 impulse.
    // 의도적으로 다양한 부호 / 크기를 섞어 적분 / 충돌 / 마찰 모든 경로를
    // 균일하게 활성화.
    const std::vector<Vec2> imp = {
        {0.0f,    0.0f},
        {200.0f, -100.0f},
        {0.0f,    0.0f},
        {-50.0f,  0.0f},
        {0.0f,    0.0f},
    };

    auto runSim = [&imp](PhysicsWorld& w, int frames) {
        for (int f = 0; f < frames; ++f) {
            if (f < static_cast<int>(imp.size())) {
                if (PhysicsBody* b = w.GetBody(1)) {
                    b->ApplyImpulse(imp[f]);
                }
            }
            w.Step(1.0f / 60.0f);
        }
    };

    runSim(w1, 600);   // 10 초.
    runSim(w2, 600);

    PhysicsBody* a = w1.GetBody(1);
    PhysicsBody* b = w2.GetBody(1);
    CR_ASSERT(a != nullptr && b != nullptr);

    // POD-like struct 의 byte 비교 — Vec2 멤버 차이를 포함해 정확히 같은
    // 비트 패턴이어야 함.
    CR_ASSERT(std::memcmp(&a->pos, &b->pos, sizeof(Vec2)) == 0);
    CR_ASSERT(std::memcmp(&a->vel, &b->vel, sizeof(Vec2)) == 0);
}
