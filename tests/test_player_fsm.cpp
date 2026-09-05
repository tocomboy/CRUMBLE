// =============================================================================
// tests/test_player_fsm.cpp — Player 단위 테스트 (3 케이스).
//
// 검증 대상:
//   - 시작 시 Airborne, 자유낙하 후 Grounded 로 전환.
//   - Grounded 상태에서 jump 키 누르면 vel.y 가 음수 큰 값.
//   - 좌/우 키 hold 시 60 frame 후 위치가 50 px 이상 이동.
//
// 케이스 + 잡는 잠재 버그:
//   1. starts_in_airborne_falls_to_grounded
//      — 초기 Airborne, 자유낙하 후 ContactCallback 이 EvaluateContact 를
//        호출해 Grounded 로 전환되어야.
//      버그: SetContactBeginCallback 미연결, EvaluateContact 의 normal
//      방향 검사 잘못.
//   2. jump_pressed_leaves_ground
//      — Grounded 에서 jumpPressed → vel.y < -300 (kJumpImp=420 - 중력 16.3).
//      버그: jumpBuffer / coyoteTimer 로직, vel.y 직접 set 누락.
//   3. walks_right_when_held
//      — 60 frame D 키 hold → x 위치 50+ px 이동.
//      버그: ApplyForce 부호 잘못, 마찰 너무 강해서 정지.
//
// 모든 테스트는 P1 단계의 tunneling 안전 패턴 (floor 두께 100) 적용.
// =============================================================================

#include "assert_eq.h"
#include "test_runner.h"
#include "../src/game/Player.h"
#include "../src/physics/PhysicsWorld.h"
#include "../src/input/InputManager.h"
#include "../src/input/Bindings.h"

#include <cmath>

// 헬퍼: 두꺼운 floor + Player 공통 셋업.
//   - floor 두께 100 (half_y=50) → tunneling 회피.
//   - InputManager 에 BindDefaultActions 호출 (액션 이름 인식 가능).
//   - 호출자가 world / im / player 를 모두 받음.
struct TestScene {
    PhysicsWorld world;
    InputManager im;
    BodyId       floorId;
    Player       player;

    TestScene(Vec2 spawnPos)
      : world(),
        im(),
        floorId(0),
        player(world, im, "p1", spawnPos)
    {
        // floor 등록 — 두께 100, 너비 2000. 폭 충분히 넓어 walks_right 시
        // 끝에 닿지 않음.
        // 주의: Player 가 먼저 생성되면 BodyId=0 이 되어버림. 본 헬퍼는
        // Player 를 멤버 초기화 마지막에 두고, 그 전에 floor 를 추가하기
        // 위해 본문에서 처리할 수 없다. 대신 Player 생성 후 floor 를 만들고
        // floorId 를 갱신.

        PhysicsBody floorDef;
        floorDef.pos   = {0, 200};
        floorDef.type  = BodyType::Static;
        floorDef.SetMass(0.0f);
        floorDef.shape = ShapeType::AABB;
        floorDef.half  = {1000, 50};       // 두께 100 (P1 tunneling fix 정합).
        floorId = world.CreateBody(floorDef);

        // ContactCallback 라우팅 — Player 의 EvaluateContact 호출.
        // 람다가 player 를 캡처. 본 클래스 인스턴스가 살아 있는 동안만 유효.
        world.SetContactBeginCallback([this](const Contact& c){
            player.EvaluateContact(c);
        });

        // 액션 매핑 등록 (jump / move_left / move_right 등 사용).
        BindDefaultActions(im);
    }
};

// 헬퍼: dt=1/60 으로 N step 실행. 각 step 은 BeginFrame → UpdateInput →
// world.Step → UpdateFSM 순서.
static void RunSteps(TestScene& s, int n,
                     SDL_Scancode pressed = SDL_SCANCODE_UNKNOWN) {
    for (int i = 0; i < n; ++i) {
        s.im.BeginFrame();
        if (pressed != SDL_SCANCODE_UNKNOWN) {
            s.im.SimulateScancode(pressed, true);
        }
        s.player.UpdateInput(1.0f / 60.0f);
        s.world.Step(1.0f / 60.0f);
        s.player.UpdateFSM(1.0f / 60.0f);
    }
}

// ---- 1. 시작 시 Airborne, 떨어져서 Grounded ----
REGISTER_TEST(player_starts_in_airborne_falls_to_grounded) {
    TestScene s(Vec2{0, 50});             // floor 위 100 px.

    // 시작은 Airborne (생성자에서 state_ = Airborne).
    CR_ASSERT(s.player.GetState() == PlayerState::Airborne);

    // 2초 (120 frame) 시뮬레이션 — 충분히 떨어져 안착 + ground 인식.
    RunSteps(s, 120);

    CR_ASSERT(s.player.GetState() == PlayerState::Grounded);
}

// ---- 2. Grounded 에서 jump 누르면 vel.y < -300 ----
REGISTER_TEST(player_jump_pressed_leaves_ground) {
    TestScene s(Vec2{0, 50});

    // 안착 (Grounded 까지 settle).
    RunSteps(s, 120);
    CR_ASSERT(s.player.GetState() == PlayerState::Grounded);

    // 점프 키 W 누르는 1 frame.
    s.im.BeginFrame();
    s.im.SimulateScancode(SDL_SCANCODE_W, true);
    s.player.UpdateInput(1.0f / 60.0f);
    s.world.Step(1.0f / 60.0f);
    s.player.UpdateFSM(1.0f / 60.0f);

    // kJumpImp=420 직접 대입 → vel.y = -420.
    // world.Step 의 중력 적분 (+16.3) 후 vel.y ≈ -403.7.
    // < -300 임계로 충분히 위로 가속 검증.
    CR_ASSERT(s.player.GetBody()->vel.y < -300.0f);
}

// ---- 3. 좌/우 이동 (D 키 60 frame) ----
REGISTER_TEST(player_walks_right_when_held) {
    TestScene s(Vec2{0, 50});

    // 안착.
    RunSteps(s, 120);

    const float startX = s.player.GetBody()->pos.x;

    // 60 frame (1초) 동안 D 키 hold.
    RunSteps(s, 60, SDL_SCANCODE_D);

    // ApplyForce + kMaxVx=300 + friction=0.6 + 60 frame 이면 50+ px 이동
    // 충분 (saturate 된 ~250 px/s 평균 × 1초 ≈ 250 px).
    CR_ASSERT(s.player.GetBody()->pos.x - startX > 50.0f);
}

// ---- 4. WallSlide — 우측 벽에 붙은 채 떨어지면 vel.y 가 cap 된다 (P6 Task 6.1) ----
//
// 시나리오:
//   - 우측 벽 (200, 300, half=5x200) 옆에 spawn — Player half=(16,24). Plan
//     원안의 spawn (180, 100) 은 우측 끝(196) 이 벽 좌측(195) 에 1 px 만
//     침투해 resolution 분리와 D-키 force 가속 사이에 ping-pong 이 발생,
//     매 step contact 보장이 안 되어 30 frame 끝 시점이 Airborne 일 수 있다.
//     본 테스트는 그 root-cause 를 피하려 spawn 을 깊게 (192, 100) 으로 옮겨
//     약 13 px 침투를 보장 — resolution 후에도 다음 frame 전에 다시 wall 과
//     contact 가 일어나 매 step WallSlide 유지. (Plan 결함 #11)
//   - D 키 hold → 우측 벽으로 계속 밀림 + 중력으로 떨어짐.
//   - UpdateFSM 의 WallSlide 진입 조건 (wallContact + falling + holdingIntoWall)
//     모두 충족.
//   - kWallSlideMaxFall=120 이 vel.y 를 그 이상 못 넘게 cap. 30 frame=0.5s 후
//     검증 — 자유낙하라면 vel.y ≈ 980*0.5 = 490 까지 가속. cap 적용 시 < 130.
//
// 잡는 잠재 버그:
//   - holdingIntoWall 검사의 부호 틀어짐.
//   - WallSlide 진입 시 vel.y cap 미적용.
//   - wallContactSignThisStep_ 가 EvaluateContact 에서 안 채워짐.
REGISTER_TEST(player_wallslide_when_falling_against_wall) {
    PhysicsWorld world;

    PhysicsBody floorDef;
    floorDef.type  = BodyType::Static;
    floorDef.SetMass(0.0f);
    floorDef.shape = ShapeType::AABB;
    floorDef.pos   = {0.0f, 600.0f};
    floorDef.half  = {1000.0f, 5.0f};
    world.CreateBody(floorDef);

    PhysicsBody wallDef;
    wallDef.type  = BodyType::Static;
    wallDef.SetMass(0.0f);
    wallDef.shape = ShapeType::AABB;
    wallDef.pos   = {200.0f, 300.0f};
    wallDef.half  = {5.0f, 200.0f};
    world.CreateBody(wallDef);

    InputManager im;
    BindDefaultActions(im);   // p1_move_right 등 액션 매핑 등록.

    // spawn (192, 100): 우측 끝 = 208, wall 좌측 = 195 → 13 px 깊은 overlap.
    // 매 step resolution 이 분리해도 D-key force 가 즉시 다시 밀어붙여 contact 유지.
    Player p(world, im, "p1", Vec2{192.0f, 100.0f});
    world.SetContactBeginCallback([&](const Contact& c) {
        p.EvaluateContact(c);
    });

    // 30 frame (0.5s) D 키 hold — Player 가 우측 벽으로 밀리면서 떨어짐.
    for (int i = 0; i < 30; ++i) {
        im.BeginFrame();
        im.SimulateScancode(SDL_SCANCODE_D, true);
        p.UpdateInput(1.0f / 60.0f);
        world.Step(1.0f / 60.0f);
        p.UpdateFSM(1.0f / 60.0f);
    }

    CR_ASSERT(p.GetState() == PlayerState::WallSlide);
    CR_ASSERT(p.GetBody()->vel.y < 130.0f);   // cap=120, 검증 임계 130 (jitter 1 step 여유).
}
