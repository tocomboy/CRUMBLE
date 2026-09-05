// =============================================================================
// src/input/InputManager.h — 액션 맵 + Pressed/Held/Released 엣지 검출.
//
// 목적:
//   - 게임 로직이 SDL scancode 가 아닌 의미 있는 액션 이름 ("p1_jump",
//     "p1_fire") 으로 입력을 쿼리하도록 추상화.
//   - 매 프레임 두 비트맵 (prev / curr) 을 비교해 Pressed (rising edge),
//     Released (falling edge), Held (현재 눌림) 세 가지 상태를 제공.
//
// 설계 흐름 ([5. Input Handling.pdf p.4] 의 하이브리드 패턴):
//   - SDL_PollEvent 는 SDL_QUIT / SDL_WINDOWEVENT 등 일회성 이벤트만 처리
//     (main.cpp 가 직접).
//   - 본 InputManager 는 [5. Input Handling.pdf p.8] 의 SDL_GetKeyboardState
//     폴링 패턴을 사용. 키 상태 배열을 매 프레임 그대로 복사.
//   - scancode 사용 ([p.7]) — 키보드 레이아웃 (QWERTY/AZERTY) 무관. 게임플레이
//     입력은 항상 scancode.
//
// 한 프레임의 호출 순서:
//   1. BeginFrame()        — prev := curr (이전 프레임의 curr 가 다음의 prev)
//   2. PumpFromSDL() 또는 SimulateScancode() — curr 갱신
//   3. IsPressed / IsHeld / IsReleased 쿼리 — 게임 로직 수행
//   (4. 다음 프레임 시작에서 다시 1 부터)
//
// 엣지 검출 산식:
//   IsPressed(action)  = ∃ binding b: curr[b] && !prev[b]    (rising edge)
//   IsReleased(action) = ∃ binding b: !curr[b] && prev[b]    (falling edge)
//   IsHeld(action)     = ∃ binding b: curr[b]
//
// Plan Revisability:
//   - [5. Input Handling.pdf p.6] 의 key.repeat 무시는 본 클래스가 직접
//     이벤트 큐를 다루지 않아 자연 회피 (GetKeyboardState 는 repeat 영향 없음).
//   - 학기 후반 PDF 가 gamepad / 더 고급 기능을 다루면 본 클래스에 추가.
// =============================================================================

#pragma once

#include <SDL.h>
#include <string>
#include <unordered_map>
#include <vector>

class InputManager {
public:
    InputManager();

    // ----- Bindings -----
    // 한 액션에 여러 scancode 를 OR 로 묶을 수 있다 (multiple_bindings_or_together).
    //   - 예: jump 액션을 W 와 SPACE 둘 다 트리거.
    //   - 같은 액션 이름에 BindAction 을 여러 번 호출해 누적.
    void BindAction(const std::string& action, SDL_Scancode code);

    // 해당 액션의 모든 binding 제거.
    void UnbindAction(const std::string& action);

    // ----- 프레임 라이프사이클 -----
    // 매 프레임 시작 시 호출 — prev := curr 스냅샷.
    //   - 이 호출 *후* 에 curr 가 갱신되어야 (PumpFromSDL 또는
    //     SimulateScancode) 그 다음 쿼리에서 curr ≠ prev 비교가 의미 있음.
    void BeginFrame();

    // 실제 SDL 키보드 상태를 curr 에 복사. 게임 실행 시 호출.
    //   - [5. Input Handling.pdf p.8] SDL_GetKeyboardState 패턴.
    void PumpFromSDL();

    // 단위 테스트용 — 특정 scancode 의 curr 값을 직접 설정.
    //   - 운영 코드는 사용 금지. 테스트만.
    void SimulateScancode(SDL_Scancode code, bool down);

    // ----- 쿼리 -----
    // 현재 눌려 있는가 (continuous 입력, 예: 좌/우 이동).
    bool IsHeld(const std::string& action) const;

    // 직전 프레임에는 안 눌렸고 이번 프레임에 처음 눌림 (one-shot 입력,
    // 예: 점프, 발사).
    bool IsPressed(const std::string& action) const;

    // 직전 프레임에는 눌렸고 이번 프레임에 떼어짐 (예: variable-height
    // jump 의 조기 컷).
    bool IsReleased(const std::string& action) const;

    // ----- 복구 -----
    // 모든 키 상태 초기화. 창 포커스 잃었을 때 사용 (P2 Task 2.3 main 참조).
    //   - 포커스 잃은 채로 키를 떼면 SDL 이 KEYUP 을 못 받아 "유령 holding"
    //     상태가 됨. 포커스 복귀 시 ResetAll 호출로 깨끗한 상태.
    void ResetAll();

private:
    // action 이름 → scancode 리스트 (OR 묶음).
    std::unordered_map<std::string, std::vector<SDL_Scancode>> bindings_;

    // SDL_NUM_SCANCODES (~512) 길이의 키 상태 배열.
    //   - bool 로 두면 1 byte * 512 = 512 byte. 매 프레임 memcpy 비용 무시 가능.
    bool curr_[SDL_NUM_SCANCODES] = {};
    bool prev_[SDL_NUM_SCANCODES] = {};
};
