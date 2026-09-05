// =============================================================================
// src/flow/GameFlow.h — 게임 전체 상태 머신 (외곽 FSM).
// =============================================================================
//
// 6 상태:
//   Title       : 시작 화면. 아무 키 → Controls.
//   Controls    : 조작법 안내. 아무 키 → RoundIntro.
//   RoundIntro  : "Round N" 카운트다운 3 초. 자동 → Playing.
//   Playing     : 실제 전투. 한 명의 HP=0 → OnRoundOver(winner) → RoundEnd.
//   RoundEnd    : 결과 표시 3 초. 점수 합산 후
//                  - 누구든 3 점 도달 → MatchEnd
//                  - 아니면          → 다음 RoundIntro
//   MatchEnd    : 매치 종료 화면 5 초 후 자동 → Title (또는 OnAnyKey 즉시).
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 3 Task 3.4.
//   - 메인 스펙 §3 GameFlow + §4 HUD 의 라운드 점수 = best-of-3.
//   - Plan Revisability: 학기 후반 PDF 가 FSM 패턴 (state pattern, hierarchical
//     FSM 등) 을 가르치면 본 단순 enum + switch 구조가 그쪽으로 정렬될 수 있음.
//
// 결정론:
//   - 모든 상태 전이는 메인 스레드의 Update / OnAnyKey / OnRoundOver 호출에서만
//     발생. 자체 시계 없음.
// =============================================================================

#pragma once

// FlowState: 외곽 6 상태. uint 미지정 → int 베이스 (스위치 호환성 우선).
enum class FlowState {
    Title,
    Controls,
    RoundIntro,
    Playing,
    RoundEnd,
    MatchEnd
};

class GameFlow {
public:
    GameFlow();

    // 매 fixed-step 호출. 자동 전이 (RoundIntro 3s, RoundEnd 3s, MatchEnd 5s).
    void Update(float dt);

    // SDL 키 입력 한 번 — Title/Controls/MatchEnd 에서 다음 상태로 진행.
    // Playing 중에는 무시 (전투 로직이 키 입력을 직접 처리).
    void OnAnyKey();

    // 한 라운드 종료 신호.
    //   - winnerIdx : 0 (P1 승) / 1 (P2 승). 인덱스로 점수 +1.
    //   - Playing 외 상태에서 호출 시 무시 (이중 트리거 방어).
    void OnRoundOver(int winnerIdx);

    // 매치 전체 리셋 — 점수 0/0, 라운드 1, 상태 Title.
    void RestartMatch();

    // -- 상태 조회 (HUD / 메인 루프 분기 용) --
    FlowState State()       const { return state_; }
    int       Round()       const { return roundNumber_; }
    int       P1Score()     const { return p1Score_; }
    int       P2Score()     const { return p2Score_; }
    int       RoundWinnerIdx() const { return roundWinnerIdx_; }
    // 매치 승자 (3 점 먼저 도달한 쪽). 없으면 -1.
    int       MatchWinnerIdx() const;
    // 현재 상태 진입 후 경과 시간 (초). HUD 의 "Round Intro 3..2..1" 카운트다운
    // 표시 등에 사용.
    float     StateTimer()  const { return stateTimer_; }

    // 메인 루프 측에서 "지금 시뮬레이션 / 입력을 굴려야 하는가?" 결정용.
    bool IsCombatActive() const { return state_ == FlowState::Playing; }

private:
    FlowState state_           = FlowState::Title;
    float     stateTimer_      = 0.0f;
    int       roundNumber_     = 1;
    int       p1Score_         = 0;
    int       p2Score_         = 0;
    int       roundWinnerIdx_  = -1;

    // 상태 전이 helper — state_ 갱신 + stateTimer_ 0 리셋.
    void Transition(FlowState s);
};
