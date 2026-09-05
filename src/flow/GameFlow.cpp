// =============================================================================
// src/flow/GameFlow.cpp — GameFlow FSM 구현.
// =============================================================================
//
// 출처:
//   - 메인 플랜 P3 Task 3.4 Step 4 의 코드 + 한국어 주석 보강.
//   - 자동 전이 시간 (3s / 3s / 5s) 은 메인 스펙 §3 의 라운드 흐름.
// =============================================================================

#include "GameFlow.h"

GameFlow::GameFlow() {
    // 모든 멤버는 클래스 내 default 초기화 — Title / 점수 0 / 라운드 1.
    // 명시적 초기화는 가독성 위해 멤버 디폴트 값에서 통일.
}

void GameFlow::Update(float dt) {
    // stateTimer_ 누적 — 모든 자동 전이의 기준.
    stateTimer_ += dt;

    switch (state_) {
        case FlowState::RoundIntro:
            // 3 초 동안 "Round N" 카운트다운 표시 후 자동으로 Playing.
            if (stateTimer_ >= 3.0f) {
                Transition(FlowState::Playing);
            }
            break;

        case FlowState::RoundEnd:
            // 3 초 동안 라운드 결과 표시 후:
            //   - 누구든 3 점 → 매치 종료 (MatchEnd).
            //   - 아니면      → 라운드 카운터 +1 + 다음 인트로.
            if (stateTimer_ >= 3.0f) {
                if (p1Score_ >= 3 || p2Score_ >= 3) {
                    Transition(FlowState::MatchEnd);
                } else {
                    ++roundNumber_;
                    Transition(FlowState::RoundIntro);
                }
            }
            break;

        case FlowState::MatchEnd:
            // 5 초 후 자동으로 Title 복귀. (사용자가 OnAnyKey 로 즉시 넘길 수도
            // 있음.) MVP 는 매치 끝나면 자동 종료가 발표 시연 흐름에 자연스럽다.
            if (stateTimer_ >= 5.0f) {
                Transition(FlowState::Title);
            }
            break;

        // Title / Controls / Playing 은 시간 기반 자동 전이 없음.
        // - Title / Controls : 키 입력으로만 전이 (OnAnyKey).
        // - Playing          : OnRoundOver 로만 전이.
        default:
            break;
    }
}

void GameFlow::OnAnyKey() {
    switch (state_) {
        case FlowState::Title:
            Transition(FlowState::Controls);
            break;
        case FlowState::Controls:
            Transition(FlowState::RoundIntro);
            break;
        case FlowState::MatchEnd:
            // 매치 종료 화면에서 키를 누르면 Title 로 — 메인 루프 측이 이때
            // RestartMatch 를 호출해 점수/라운드를 초기화하는 책임을 진다.
            Transition(FlowState::Title);
            break;
        default:
            // RoundIntro / Playing / RoundEnd 에서는 키 입력 무시.
            // (Playing 의 키 입력은 전투 시스템이 따로 소비.)
            break;
    }
}

void GameFlow::OnRoundOver(int winnerIdx) {
    // Playing 외에서 호출되면 무시 — main.cpp 가 HP <= 0 check 를 fixed-step
    // 안에서 매번 돌리므로, 한 라운드에 여러 번 호출될 수 있다. 이중 트리거
    // 방어.
    if (state_ != FlowState::Playing) return;
    roundWinnerIdx_ = winnerIdx;
    if      (winnerIdx == 0) ++p1Score_;
    else if (winnerIdx == 1) ++p2Score_;
    // -1 등 비정상 winnerIdx 는 무시 (점수 변화 없음, 그러나 RoundEnd 로는 넘어감).

    Transition(FlowState::RoundEnd);
}

void GameFlow::RestartMatch() {
    // 점수 / 라운드 / 결과 모두 초기화 + 상태 Title.
    p1Score_         = 0;
    p2Score_         = 0;
    roundNumber_     = 1;
    roundWinnerIdx_  = -1;
    Transition(FlowState::Title);
}

int GameFlow::MatchWinnerIdx() const {
    if (p1Score_ >= 3) return 0;
    if (p2Score_ >= 3) return 1;
    return -1;   // 아직 매치 미종료.
}

void GameFlow::Transition(FlowState s) {
    state_      = s;
    stateTimer_ = 0.0f;
    // roundWinnerIdx_ 는 일부러 초기화 안 함 — RoundEnd 동안 HUD 가 계속 쓸 수
    // 있어야 함. 다음 OnRoundOver 호출 시 덮어쓰여진다.
}
