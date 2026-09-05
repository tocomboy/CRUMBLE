// =============================================================================
// src/core/CooldownTimer.cpp — CooldownTimer 의 구현.
//
// 출처: [3. Game Loop.pdf p.9] 의 코드를 PascalCase + duration 0 케이스 안전
//       처리로 보강. 진행률 산식 (1 - remaining/duration) 은 PDF 그대로.
// =============================================================================

#include "CooldownTimer.h"

#include <algorithm>     // std::max — Progress() 에서 음수 클램프에 사용.

CooldownTimer::CooldownTimer(float duration)
    : duration_(duration),    // 한 번 정해지면 불변.
      remaining_(0.0f)        // 초기 상태: 즉시 ready (Start() 호출 전).
{
    // 본문 비움. 멤버 초기화 리스트로 충분.
}

void CooldownTimer::Start() {
    // 쿨다운 발동 = remaining_ 을 duration_ 으로 리필.
    // 이 시점부터 IsReady() == false, Progress() == 0.
    remaining_ = duration_;
}

void CooldownTimer::Update(float dt) {
    // 이미 ready 상태 (remaining_ <= 0) 면 아무것도 안 한다.
    // remaining_ 이 음수로 떨어지는 것을 방지 — 0 에서 멈춰야 Progress 가
    // 1.0 을 넘지 않는다.
    if (remaining_ > 0.0f) {
        remaining_ -= dt;
        // 작은 양으로 0 을 살짝 넘는 부동소수 케이스를 0 으로 클램프.
        // (예: remaining_ = 0.001, dt = 0.0167 → -0.0157 가 되는데, 다음
        //  Progress() 호출에서 std::max 가 0 으로 잡아주지만 명시적으로
        //  여기서 처리해 두면 디버거에서 보기도 깔끔하다.)
        if (remaining_ < 0.0f) {
            remaining_ = 0.0f;
        }
    }
}

void CooldownTimer::Reset() {
    // 강제 ready. 라운드 종료 시 / 게임 재시작 시 무기 쿨다운을 한 번에 깨끗
    // 하게 만들기 위함.
    remaining_ = 0.0f;
}

bool CooldownTimer::IsReady() const {
    // 부동소수 비교는 정확히 0 일 필요는 없다 — Update 에서 이미 음수를
    // 0 으로 잡았으므로 == 0 또는 약간의 잔여 음수가 들어와도 안전.
    return remaining_ <= 0.0f;
}

float CooldownTimer::Progress() const {
    // 엣지 케이스: duration_ 이 0 이하라면 의미 있는 진행률을 정의할 수 없다.
    // 호출자 안전을 위해 1.0 (즉시 완료) 으로 처리.
    if (duration_ <= 0.0f) {
        return 1.0f;
    }
    // remaining_ 이 음수일 가능성을 std::max(.., 0) 로 차단한 뒤 비율 계산.
    //   r = remaining/duration ∈ [0, 1]
    //   progress = 1 - r ∈ [0, 1]
    // PDF p.9 의 식 그대로.
    float r = std::max(remaining_, 0.0f) / duration_;
    return 1.0f - r;
}

float CooldownTimer::Remaining() const {
    // 음수가 들어가 있어도 그대로 반환 (디버깅 시 어떤 dt 에 어디까지
    // 떨어졌는지 보고 싶을 수 있다). 호출자가 UI 표시 용도라면 직접
    // std::max(.., 0) 적용 권장.
    return remaining_;
}
