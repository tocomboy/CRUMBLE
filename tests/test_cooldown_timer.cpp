// =============================================================================
// tests/test_cooldown_timer.cpp — CooldownTimer 단위 테스트 4 케이스.
//
// 검증 대상:
//   - src/core/CooldownTimer (PDF [3. Game Loop.pdf p.9] 패턴 구현).
//
// 4 케이스 전략:
//   1. cooldown_starts_ready              — 생성 직후 IsReady, Progress=1.
//   2. cooldown_after_start_not_ready     — Start() 직후 IsReady=false, Progress=0.
//   3. cooldown_progress_advances         — Update(dt) 가 Progress 를 정확히 증가.
//   4. cooldown_completes                 — duration 초과 dt 로 Update 시 ready.
//
// 각 테스트가 잡는 잠재 버그:
//   1번  → 생성자가 remaining_ 을 0 으로 안 두면 IsReady() 가 false 로 시작.
//   2번  → Start 가 remaining_ 을 갱신 안 하면 본 케이스가 곧장 깨짐.
//   3번  → Progress 식이 (remaining/duration) 같이 부호가 뒤집히면 0.4 가 0.6
//          처럼 나옴.
//   4번  → Update 의 음수 클램프가 빠져 있으면 Progress 가 1.0 을 초과 (예:
//          1.5 returned), HUD 게이지가 화면 밖으로 그려짐.
// =============================================================================

#include "assert_eq.h"
#include "test_runner.h"
#include "../src/core/CooldownTimer.h"

// ---- 1. 생성 직후 즉시 ready 상태 ----
REGISTER_TEST(cooldown_starts_ready) {
    // 0.5 초 쿨다운으로 만들지만 아직 Start() 호출 전.
    CooldownTimer t(0.5f);
    // 호출 전이므로 remaining_ == 0, IsReady() == true 이어야 한다.
    CR_ASSERT(t.IsReady());
    // Progress 는 1.0 (= 완료) 으로 표기.
    // 1e-6 eps: float 계산 오차 한계.
    CR_NEAR(t.Progress(), 1.0f, 1e-6f);
}

// ---- 2. Start 직후 ready 가 아닌 상태 ----
REGISTER_TEST(cooldown_after_start_not_ready) {
    CooldownTimer t(0.5f);
    t.Start();                          // 쿨다운 시작 — remaining_ = 0.5.
    CR_ASSERT(!t.IsReady());            // 아직 ready 아님.
    CR_NEAR(t.Progress(), 0.0f, 1e-6f); // 막 시작했으므로 진행률 0.
}

// ---- 3. Update 로 진행률이 정확히 증가하는지 ----
REGISTER_TEST(cooldown_progress_advances) {
    CooldownTimer t(1.0f);              // 1 초 쿨다운.
    t.Start();
    t.Update(0.4f);                      // 0.4 초 경과.
    // remaining_ = 1.0 - 0.4 = 0.6 → Progress = 1 - 0.6/1.0 = 0.4.
    // float 누적 오차를 감안해 1e-5 까지 허용.
    CR_NEAR(t.Progress(), 0.4f, 1e-5f);
    // 아직 0.6 초 남았으므로 ready 아님.
    CR_ASSERT(!t.IsReady());
}

// ---- 4. duration 초과 dt 로 Update 시 ready 로 전환되고 progress 가 1.0 에서
//     멈춤 (1.0 초과 안 함) ----
REGISTER_TEST(cooldown_completes) {
    CooldownTimer t(1.0f);
    t.Start();
    t.Update(1.5f);                      // 의도적으로 duration 초과.
    CR_ASSERT(t.IsReady());              // ready 로 전환.
    // Update 안의 0 클램프가 있어야 Progress 가 1.0 에 정확히 멈춤.
    // 클램프 누락 시 1.5 또는 그 비례값이 나와 본 검증이 깨진다.
    CR_NEAR(t.Progress(), 1.0f, 1e-6f);
}
