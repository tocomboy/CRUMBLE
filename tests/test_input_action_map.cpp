// =============================================================================
// tests/test_input_action_map.cpp — InputManager 4 케이스.
//
// 검증 대상:
//   - rising edge (Pressed) 가 BeginFrame 후 1 번만 true.
//   - falling edge (Released) 가 키 떼는 프레임에 true.
//   - 한 액션에 여러 scancode 를 묶었을 때 OR 동작.
//   - ResetAll 이 모든 상태 즉시 초기화.
//
// 호출 순서 모델 (BeginFrame 모델, 플랜 Step 4c):
//   1) BeginFrame()                 // prev := curr
//   2) Simulate / Pump               // curr 갱신
//   3) IsPressed/Held/Released 쿼리
// =============================================================================

#include "assert_eq.h"
#include "test_runner.h"
#include "../src/input/InputManager.h"

REGISTER_TEST(action_pressed_only_once_on_rising_edge) {
    InputManager im;
    im.BindAction("jump", SDL_SCANCODE_W);

    // 프레임 1: 키 떼인 채로 시작.
    im.BeginFrame();                              // prev=0, curr=0.
    im.SimulateScancode(SDL_SCANCODE_W, false);   // curr=0 (변화 없음).
    CR_ASSERT(!im.IsPressed("jump"));
    CR_ASSERT(!im.IsHeld("jump"));

    // 프레임 2: 키 누름 (rising edge 발생).
    im.BeginFrame();                              // prev := curr (둘 다 0).
    im.SimulateScancode(SDL_SCANCODE_W, true);    // curr=1.
    // curr=1 && !prev=0 → IsPressed true.
    CR_ASSERT(im.IsPressed("jump"));
    CR_ASSERT(im.IsHeld("jump"));
    CR_ASSERT(!im.IsReleased("jump"));

    // 프레임 3: 키 계속 눌려 있음 (rising edge 아님).
    im.BeginFrame();                              // prev := curr (둘 다 1).
    // curr=1 && !prev=1 → false.
    CR_ASSERT(!im.IsPressed("jump"));
    // 여전히 눌려 있음.
    CR_ASSERT(im.IsHeld("jump"));
}

REGISTER_TEST(action_released_falling_edge) {
    InputManager im;
    im.BindAction("fire", SDL_SCANCODE_F);

    // 프레임 1: 키 누름 — curr=1.
    im.BeginFrame();
    im.SimulateScancode(SDL_SCANCODE_F, true);

    // 프레임 2: 키 떼기 — curr=0, prev=1 (BeginFrame 의 스냅샷).
    im.BeginFrame();                              // prev := 1.
    im.SimulateScancode(SDL_SCANCODE_F, false);   // curr := 0.
    // !curr=1 && prev=1 → IsReleased true.
    CR_ASSERT(im.IsReleased("fire"));
    CR_ASSERT(!im.IsHeld("fire"));
}

REGISTER_TEST(multiple_bindings_or_together) {
    InputManager im;
    // 같은 액션에 두 scancode — OR 결합.
    im.BindAction("jump", SDL_SCANCODE_W);
    im.BindAction("jump", SDL_SCANCODE_SPACE);

    im.BeginFrame();
    // SPACE 만 눌러도 jump 액션은 Pressed.
    im.SimulateScancode(SDL_SCANCODE_SPACE, true);
    CR_ASSERT(im.IsPressed("jump"));
}

REGISTER_TEST(reset_clears_all_held) {
    InputManager im;
    im.BindAction("right", SDL_SCANCODE_D);

    im.BeginFrame();
    im.SimulateScancode(SDL_SCANCODE_D, true);
    CR_ASSERT(im.IsHeld("right"));

    // ResetAll 후 모든 상태가 0.
    im.ResetAll();
    im.BeginFrame();                              // prev=0 (이미 reset 됨).
    // 별도 Simulate 없이도 curr 가 0 이라 held 가 아님.
    CR_ASSERT(!im.IsHeld("right"));
}
