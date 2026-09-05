// =============================================================================
// test_mortar_aimstate.cpp — MortarAimState dual oscillation 단위 테스트.
// =============================================================================
//
// 검증 4 가지:
//   1) 초기 상태 (phase=0) 에서 speed = kMinSpeed.
//   2) 게이지 phase 0.5 (반주기) 에서 speed = kMaxSpeed.
//      그 후 phase 1.0 까지 다시 진행하면 kMinSpeed 로 복귀.
//   3) 각도 phase 0.25 (1/4 주기, sine 의 첫 피크) 에서 elevation 의 진폭
//      변화가 명확 (라디안 0.5 이상). Plan 결함 #9 — 원본 plan 의 0.75s
//      (반주기) 호출은 sin(π)=0 이라 변화량 0 으로 검증 실패.
//   4) facingSign +1/-1 가 CurrentDirection 의 x 부호만 미러, y 는 동일.
//
// 출처: 메인 플랜 P5 Task 5.3 Step 1 의 4 케이스 + Plan 결함 #9 fix
//       (anglePhase 의 검증 시점을 sine 의 첫 피크로 이동).
// =============================================================================

#include <cmath>

#include "test_runner.h"
#include "assert_eq.h"

#include "../src/game/weapons/MortarAimState.h"

REGISTER_TEST(mortar_initial_speed_is_min) {
    MortarAimState s;
    s.facingSign = +1;
    // 초기 gaugePhase=0 → cos(0)=1 → t=0 → speed=kMinSpeed.
    CR_NEAR(s.CurrentSpeed(), MortarAimState::kMinSpeed, 1e-3f);
}

REGISTER_TEST(mortar_gauge_oscillates) {
    MortarAimState s;
    s.facingSign = +1;
    // 0.5s of 1.0s period → phase=0.5 → cos(π)=-1 → t=1 → speed=kMaxSpeed.
    s.AdvanceGauge(0.5f);
    CR_NEAR(s.CurrentSpeed(), MortarAimState::kMaxSpeed, 1.0f);
    // 추가 0.5s → phase=1.0 → cos(2π)=1 → t=0 → speed=kMinSpeed.
    s.AdvanceGauge(0.5f);
    CR_NEAR(s.CurrentSpeed(), MortarAimState::kMinSpeed, 1.0f);
}

REGISTER_TEST(mortar_angle_oscillates_around_base) {
    MortarAimState s;
    s.facingSign = +1;
    // 초기 elevation = base (45°) — sin(0) = 0 이라 진폭 = 0.
    float e0 = s.CurrentElevation();
    // [Plan 결함 #9 fix]
    //   원안: AdvanceAngle(0.75f) — 0.75/1.5 = phase 0.5 → sin(π)=0 → 변화량 0.
    //   sine 기반 oscillation 의 첫 피크는 phase 0.25 → sin(π/2)=1 이므로
    //   본 테스트는 0.25 × kAnglePeriod = 0.375s 만큼 진행해야 진폭의
    //   첫 피크에 도달한다.
    //   원본 plan 의 의도 (반주기 후 변화량 검증) 와 코드 산식이 충돌하므로
    //   root-cause fix 로 호출 인자를 sine 의 첫 피크 시점으로 이동.
    s.AdvanceAngle(0.375f);
    float e1 = s.CurrentElevation();
    // 진폭 ≈ 60° = 1.047 rad. 임계 0.5 rad 면 노이즈 위로 충분.
    CR_ASSERT(std::fabs(e1 - e0) > 0.5f);
}

REGISTER_TEST(mortar_direction_facing_sign_flips_x) {
    MortarAimState s1;
    s1.facingSign = +1;
    MortarAimState s2;
    s2.facingSign = -1;
    Vec2 d1 = s1.CurrentDirection();
    Vec2 d2 = s2.CurrentDirection();
    // x 는 부호 반전 (cos(e) × facingSign).
    CR_NEAR(d1.x, -d2.x, 1e-5f);
    // y 는 동일 (-sin(e) 는 facingSign 무관).
    CR_NEAR(d1.y, d2.y, 1e-5f);
}
