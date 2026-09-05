// =============================================================================
// src/game/weapons/MortarAimState.h — 박격포 조준 상태 (R1 dual oscillation).
// =============================================================================
//
// 목적:
//   - 박격포의 R1 (필수 1차 구현) 사양을 따른다 — 게이지 (속도) 와 각도가
//     각각 독립된 sine 파로 진동. 플레이어가 발사 키를 눌러 게이지를, 별도
//     각도 키를 눌러 각도를 멈추고 발사 시점에 두 값으로 포물선을 발사.
//
// 핵심 산식:
//   - kBaseElevation = 70° (기본 발사각, 위쪽 = -y 방향).
//     이력: 45° (P5 최초) → 60° (P7 시각 검증 1차) → 70° (P7 시각 검증 최종).
//   - kAngleAmp      = ±60° (sin 진폭 — 최대 10°/+130° 까지 변동).
//   - kGaugePeriod   = 1.0 s (게이지 한 사이클 시간).
//   - kAnglePeriod   = 1.5 s (각도 한 사이클 시간).
//   - kMinSpeed      = 400 픽셀/s (게이지 최저 시 발사 속도).
//   - kMaxSpeed      = 1100 픽셀/s (게이지 최고 시 발사 속도).
//
//   Speed(phase)     = kMinSpeed + (kMaxSpeed - kMinSpeed)
//                                 × (0.5 - 0.5 × cos(phase × 2π))
//   Elevation(phase) = kBaseElevation + sin(phase × 2π) × kAngleAmp
//   Direction(e)     = (cos(e) × facingSign, -sin(e))    // -y = 위쪽
//
//   - Speed 가 cos 기반인 이유: phase=0 에서 speed=kMinSpeed (시작 시점에
//     "꽉 차지 않은 게이지"), phase=0.5 에서 kMaxSpeed (반주기에 최대).
//     사용자에게 "충전" 느낌을 주는 표준 진동 패턴.
//   - Elevation 이 sin 기반인 이유: phase=0 에 base (70°), 1/4 주기에 +피크
//     (130°), 반주기에 다시 base (70°), 3/4 주기에 -피크 (10°).
//
// facingSign:
//   - +1 = 우측, -1 = 좌측. CurrentDirection 의 x 부호만 바뀌고 y 는 동일
//     (수직 발사각은 좌우 동일).
//   - Player::SetFacingSign 이 본 필드를 갱신.
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 5 Task 5.3.
//   - kPi 는 src/math/Vec2.h.
//   - Plan Revisability Policy 노트: Week 9 (Numerical Analysis 1) PDF 가
//     도착하면 sin/cos 보간 패턴을 강의 패턴으로 정렬.
//
// 결정론:
//   - 본 struct 는 외부에서 dt 를 받아 phase 를 누적하는 순수 산식 — RNG 없음.
//   - sin/cos 이 결정론적 (-ffast-math 금지 + IEEE-754) → 같은 phase 에 같은
//     출력.
// =============================================================================

#pragma once

#include "../../math/Vec2.h"

struct MortarAimState {
    // 누적 phase. dt 호출마다 dt / kPeriod 만큼 증가.
    //   - 1.0 이 한 주기. 컴파일러는 부동소수점 누적의 정밀도 손실을
    //     걱정해야 하지만, 본 게임의 한 라운드 ~30s 동안 약 30 사이클이라
    //     충분히 안정.
    float gaugePhase = 0.0f;
    float anglePhase = 0.0f;
    // facingSign: ±1. CurrentDirection 의 x 부호.
    int   facingSign = +1;

    // -- 진동 파라미터 (P5 R1 사양 + P7 시각 검증 단계 base 2회 상향) --
    //   - kBaseElevation 이력:
    //       45° (P5 최초) → 60° (P7 1차: 사거리 부족 개선)
    //                     → 70° (P7 최종: 60° 에서도 최저 게이지 시 거리 여전히
    //                             짧다는 시각 피드백 → 추가 상향).
    //     현재 70° 로 변동 범위 10°~130° — 중심이 거의 수직에 가까워
    //     박격포 본연의 "높이 쏘아 멀리 던지는" 느낌 구현.
    //   - kAngleAmp: 60° 유지 — base 70° 기준 변동 범위 10°(최저)~130°(최고).
    //     130° 는 약간 후방이지만 게이지-각도 동시 조작 특성상 후방 사격은
    //     드물다. 단위 테스트는 amp 변화량만 검증 (|Δe| > 0.5 rad) 이므로
    //     base 변경에 영향 없음.
    static constexpr float kBaseElevation = 70.0f * kPi / 180.0f;   // 1.2217 rad.
    static constexpr float kAngleAmp      = 60.0f * kPi / 180.0f;   // 1.0472 rad.
    static constexpr float kGaugePeriod   = 1.0f;                    // 초.
    static constexpr float kAnglePeriod   = 1.5f;                    // 초.
    static constexpr float kMinSpeed      = 400.0f;                  // 픽셀/s.
    static constexpr float kMaxSpeed      = 1100.0f;                 // 픽셀/s.

    // 양 phase 를 0 으로 — Player::EnterMortarAiming / Mortar::OnSwitchedOut
    // / 발사 직후 호출. facingSign 은 보존.
    void Reset() { gaugePhase = 0.0f; anglePhase = 0.0f; }

    // dt (초) 만큼 phase 증가. 호출자가 0 이상 dt 를 보장.
    //   - 매 fixed-step (1/60s) 호출 시 게이지는 1/60 / 1.0 = 1/60 phase 증가
    //     → 1 초에 한 사이클 완성.
    //   - 본 게임에서는 Player::UpdateInput 의 MortarAiming 분기가 fire
    //     키 hold 중에만 호출 → 사용자가 누른 시간만큼 게이지가 "굴러간다".
    void AdvanceGauge(float dt) { gaugePhase += dt / kGaugePeriod; }
    void AdvanceAngle(float dt) { anglePhase += dt / kAnglePeriod; }

    // 현재 elevation 각도 (라디안). 0 = 수평 우측, +π/2 = 수직 위.
    float CurrentElevation() const;

    // 현재 발사 방향 단위 벡터 (정확히 단위 길이는 아니지만 |v| ≈ 1).
    //   - x = cos(e) × facingSign  (좌우 미러)
    //   - y = -sin(e)              (위쪽 = -y)
    // facingSign 만 좌우 부호. y 는 항상 위쪽 (-).
    Vec2  CurrentDirection() const;

    // 현재 발사 속도 (픽셀/초). kMinSpeed ~ kMaxSpeed.
    float CurrentSpeed() const;
};
