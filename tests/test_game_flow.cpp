// =============================================================================
// test_game_flow.cpp — GameFlow 6-state FSM 단위 테스트.
// =============================================================================
//
// 검증 5 가지:
//   1) 시작 상태 = Title.
//   2) OnAnyKey 두 번 → Title → Controls → RoundIntro. roundNumber_ = 1.
//   3) RoundIntro 가 정확히 3 초 후 Playing 으로 자동 전이.
//   4) Playing 중 OnRoundOver(0) 호출 시 RoundEnd + p1Score+1, 3 초 후
//      RoundIntro 로 자동 진행 + roundNumber_ = 2.
//   5) 3 라운드 연속 P1 승리 → MatchEnd + MatchWinnerIdx = 0.
//
// 출처: 메인 플랜 P3 Task 3.4 Step 5 그대로.
// =============================================================================

#include "test_runner.h"
#include "assert_eq.h"

#include "../src/flow/GameFlow.h"

REGISTER_TEST(flow_starts_title) {
    GameFlow f;
    CR_ASSERT(f.State() == FlowState::Title);
    CR_ASSERT(f.Round() == 1);
    CR_ASSERT(f.P1Score() == 0);
    CR_ASSERT(f.P2Score() == 0);
}

REGISTER_TEST(flow_title_to_controls_to_roundintro) {
    GameFlow f;
    f.OnAnyKey();
    CR_ASSERT(f.State() == FlowState::Controls);
    f.OnAnyKey();
    CR_ASSERT(f.State() == FlowState::RoundIntro);
    CR_ASSERT(f.Round() == 1);
}

REGISTER_TEST(flow_roundintro_advances_after_3s) {
    GameFlow f;
    f.OnAnyKey();   // Title → Controls
    f.OnAnyKey();   // Controls → RoundIntro

    // 2.999s 까지는 아직 RoundIntro.
    f.Update(2.999f);
    CR_ASSERT(f.State() == FlowState::RoundIntro);

    // 임계 3.0s 를 살짝 넘는 순간 Playing.
    f.Update(0.01f);
    CR_ASSERT(f.State() == FlowState::Playing);
}

REGISTER_TEST(flow_round_over_triggers_roundend_then_next_round) {
    GameFlow f;
    f.OnAnyKey();         // Title → Controls
    f.OnAnyKey();         // Controls → RoundIntro
    f.Update(3.0f);       // RoundIntro → Playing (3s 누적, 임계 도달).

    f.OnRoundOver(0);     // P1 승 → RoundEnd, p1Score_ = 1.
    CR_ASSERT(f.State() == FlowState::RoundEnd);
    CR_ASSERT(f.P1Score() == 1);

    // 3 초 경과 → 점수 미만 3 → 다음 RoundIntro.
    f.Update(3.0f);
    CR_ASSERT(f.State() == FlowState::RoundIntro);
    CR_ASSERT(f.Round() == 2);
}

REGISTER_TEST(flow_three_wins_match_end) {
    GameFlow f;
    f.OnAnyKey();
    f.OnAnyKey();
    f.Update(3.0f);                 // RoundIntro 3s → Playing.

    // 3 라운드 연속 P1 승 시나리오.
    //
    // [플랜 결함 수정 노트]
    //   메인 플랜 P3 Task 3.4 의 원본 테스트는 라운드 사이의 RoundIntro 3s
    //   자동 전이를 시뮬레이트하지 않아, 두 번째 OnRoundOver 가 RoundIntro
    //   상태에서 호출되어 무시되었다. OnRoundOver 는 Playing 외 상태에서 무시
    //   되도록 의도적으로 설계됐기 때문 (이중 트리거 방어).
    //   본 수정: 라운드 마다 RoundEnd 3s → RoundIntro 3s → Playing 의 두 단계
    //   Update 를 명시적으로 진행. 마지막 (3 라운드) 만 RoundIntro 단계 없이
    //   MatchEnd 로 직행해야 하므로 분기.
    for (int i = 0; i < 3; ++i) {
        f.OnRoundOver(0);    // Playing → RoundEnd (p1Score++).
        f.Update(3.0f);      // RoundEnd 3s 경과:
                             //   - 마지막 라운드: p1Score_=3 → MatchEnd 직행.
                             //   - 그 외:        RoundIntro 로.
        if (i < 2) {
            f.Update(3.0f);  // RoundIntro 3s → Playing (다음 라운드 시작).
        }
    }
    CR_ASSERT(f.State() == FlowState::MatchEnd);
    CR_ASSERT(f.MatchWinnerIdx() == 0);
    CR_ASSERT(f.P1Score() == 3);
}
