// =============================================================================
// src/input/InputManager.cpp — 구현.
//
// 출처 패턴:
//   - [5. Input Handling.pdf p.8] SDL_GetKeyboardState 폴링.
//   - prev/curr 두 비트맵 비교는 일반 게임 입력 abstraction 의 표준 패턴
//     (PDF 후반부에서 직접 다룰지 미지수, 도착 시 정렬).
// =============================================================================

#include "InputManager.h"

#include <cstring>     // std::memcpy, std::memset.

InputManager::InputManager() {
    // 기본 생성자 — bindings_ 는 빈 맵, curr_/prev_ 는 zero-init.
    // 멤버 초기화 리스트로 충분하므로 본문 비움.
}

void InputManager::BindAction(const std::string& action, SDL_Scancode code) {
    // unordered_map 은 빈 키에 push_back 호출 시 자동 vector 생성.
    bindings_[action].push_back(code);
}

void InputManager::UnbindAction(const std::string& action) {
    // 액션 자체를 erase 해도 되지만 vector 만 비워 두면 [] 접근 시 빈
    // 리스트로 안전하게 동작.
    bindings_[action].clear();
}

void InputManager::BeginFrame() {
    // 핵심 — 이전 프레임의 curr 를 다음 비교의 prev 로.
    // 호출 순서: BeginFrame → PumpFromSDL/Simulate (curr 갱신) → 쿼리.
    std::memcpy(prev_, curr_, sizeof(curr_));
}

void InputManager::PumpFromSDL() {
    // [5. Input Handling.pdf p.8] SDL 이 관리하는 내부 배열 포인터.
    // numkeys 는 nullptr 로 두어 무시 (커스텀 길이 안 받음).
    const Uint8* sdl = SDL_GetKeyboardState(nullptr);
    for (int i = 0; i < SDL_NUM_SCANCODES; ++i) {
        // SDL 의 Uint8 (0 또는 1) 을 bool 로. 0 이 아니면 true.
        curr_[i] = (sdl[i] != 0);
    }
}

void InputManager::SimulateScancode(SDL_Scancode code, bool down) {
    // 테스트용 직접 키 상태 조작. 운영 코드는 PumpFromSDL 만.
    curr_[code] = down;
}

void InputManager::ResetAll() {
    // 양쪽 모두 0 으로 → 다음 BeginFrame 후의 첫 쿼리 결과가 모두 false.
    std::memset(curr_, 0, sizeof(curr_));
    std::memset(prev_, 0, sizeof(prev_));
}

// 헬퍼 함수 (file-local) — bindings_ 에서 액션을 찾아 모든 scancode 에
// 대해 술어 (predicate) 를 적용, OR 결합한 결과 반환.
//   - 람다 캡처 [&] 로 InputManager 의 const 멤버 접근.
//   - 액션이 등록 안 됐으면 false.
//
// IsHeld / IsPressed / IsReleased 가 동일 패턴이라 헬퍼로 추상화 가능하나,
// 각 메서드의 술어가 짧아 읽기 좋게 인라인 작성.

bool InputManager::IsHeld(const std::string& action) const {
    auto it = bindings_.find(action);
    if (it == bindings_.end()) return false;
    for (SDL_Scancode c : it->second) {
        if (curr_[c]) return true;
    }
    return false;
}

bool InputManager::IsPressed(const std::string& action) const {
    auto it = bindings_.find(action);
    if (it == bindings_.end()) return false;
    for (SDL_Scancode c : it->second) {
        // rising edge: curr=true && prev=false.
        // 한 액션의 여러 binding 중 하나라도 rising edge 면 true.
        if (curr_[c] && !prev_[c]) return true;
    }
    return false;
}

bool InputManager::IsReleased(const std::string& action) const {
    auto it = bindings_.find(action);
    if (it == bindings_.end()) return false;
    for (SDL_Scancode c : it->second) {
        // falling edge: curr=false && prev=true.
        if (!curr_[c] && prev_[c]) return true;
    }
    return false;
}
