// =============================================================================
// src/game/weapons/MortarAimState.cpp — sin/cos 기반 박격포 조준 보간.
// =============================================================================
//
// 출처:
//   - 메인 플랜 P5 Task 5.3 의 코드 + 본 프로젝트 컨벤션 (한국어 주석) 보강.
// =============================================================================

#include "MortarAimState.h"

#include <cmath>

float MortarAimState::CurrentElevation() const {
    // sine 기반 oscillation:
    //   phase 0 → sin(0) = 0     → elevation = base (45°)
    //   phase 0.25 → sin(π/2) = 1 → elevation = base + amp (105°)
    //   phase 0.5  → sin(π) = 0   → elevation = base (45°)
    //   phase 0.75 → sin(3π/2)=-1 → elevation = base - amp (-15°)
    return kBaseElevation + std::sin(anglePhase * 2.0f * kPi) * kAngleAmp;
}

Vec2 MortarAimState::CurrentDirection() const {
    float e = CurrentElevation();
    // y = -sin(e) — 화면 좌표계에서 -y 가 위쪽이므로 발사가 자연스럽게 상방.
    // x = cos(e) * facingSign — 좌우 부호만 facingSign 으로 미러링.
    return Vec2{std::cos(e) * static_cast<float>(facingSign),
                -std::sin(e)};
}

float MortarAimState::CurrentSpeed() const {
    // cos 기반 0~1 보간:
    //   phase 0   → cos(0) = 1  → t = 0 → speed = kMinSpeed.
    //   phase 0.5 → cos(π) = -1 → t = 1 → speed = kMaxSpeed.
    //   phase 1   → cos(2π) = 1 → t = 0 → speed = kMinSpeed.
    // 이 패턴은 (1 - cos)/2 의 표준 동작 — 0 에서 시작해 반주기에 1 도달
    // 후 다시 0.
    float t = 0.5f - 0.5f * std::cos(gaugePhase * 2.0f * kPi);
    return kMinSpeed + (kMaxSpeed - kMinSpeed) * t;
}
