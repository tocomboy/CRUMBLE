// =============================================================================
// src/core/CooldownTimer.h — duration / remaining 패턴의 쿨다운 타이머.
//
// 목적:
//   - 무기 발사 쿨다운 (P3 Rifle, P5 Shotgun / Mortar 의 reload),
//   - 라운드 인트로 3초 카운트다운 (P3),
//   - 박격포 게이지 oscillation 시간 단위 (P5)
//   등 "고정된 duration 만큼 카운트 다운한다" 가 필요한 모든 곳에서 재사용.
//
// 출처:
//   - [3. Game Loop.pdf p.9] CooldownTimer.
//     PDF 의 클래스명 / 필드명 / 진행률 산식 (1 - remaining/duration) 을
//     그대로 차용. 메서드는 PascalCase 로 통일.
//   - 호출 예 (P3 Rifle):
//       CooldownTimer reload(2.0f);
//       if (firePressed && reload.IsReady()) { fire(); reload.Start(); }
//       reload.Update(dt);
//
// 결정론:
//   - 본 클래스는 호출자가 dt 를 명시적으로 전달하므로 자체 시계가 없다.
//     fixed-timestep 메인 루프에서 `Update(kFixedDt)` 로 호출하면 결정론적.
// =============================================================================

#pragma once

class CooldownTimer {
public:
    // 생성자: duration 은 한 번 정해지면 변하지 않는 고정 쿨다운 시간 (초).
    //   - 예: CooldownTimer reload(2.0f) 는 2 초 쿨다운.
    //   - explicit: 정수/실수가 의도치 않게 CooldownTimer 로 묵시 변환되는
    //     사고 방지 ("CooldownTimer t = 2.0f;" 가 오류로 잡힘).
    explicit CooldownTimer(float duration);

    // 쿨다운을 발동시켜 remaining_ 을 duration_ 으로 채운다.
    //   - 사용 시점: 무기 발사 직후, 라운드 인트로 시작 시 등.
    //   - 호출 후 IsReady() 는 false 로, Progress() 는 0 으로.
    void Start();

    // dt (초) 만큼 remaining_ 을 감소시킨다.
    //   - dt < 0 은 호출자 책임 (음수 입력 시 쿨다운이 거꾸로 가는 비정상
    //     상태 — 일부러 막지 않음, 디버깅 시 빠르게 발견되도록).
    //   - remaining_ 가 0 이하로 내려가면 0 으로 클램프 (Progress() 가 1.0
    //     초과로 튀는 것을 방지).
    void Update(float dt);

    // remaining_ 을 즉시 0 으로. IsReady() 즉시 true.
    //   - 사용 시점: 라운드 종료 시 모든 쿨다운 일괄 초기화.
    void Reset();

    // 쿨다운이 끝났는가? remaining_ <= 0 이면 true.
    //   - 호출 가능 여부 분기에 사용:
    //       if (cooldown.IsReady() && firePressed) { ... }
    bool IsReady() const;

    // 진행률 0.0 ~ 1.0.
    //   - 0.0 = Start() 직후 (= 쿨다운 막 시작).
    //   - 1.0 = 쿨다운 완료 (= 다시 발사 가능).
    //   - HUD 의 reload 게이지 (P3 HUD) 가 이 값을 그대로 0~1 로 사용.
    //   - duration_ <= 0 이면 1.0 반환 (즉시 ready 인 케이스).
    float Progress() const;

    // 남은 쿨다운 시간 (초). UI 에 "장전 0.5s" 식 표기 시 사용.
    float Remaining() const;

    // 생성자에서 받은 duration_ (한 번 정해지면 불변). HUD / 디버그 표시용.
    float Duration() const { return duration_; }

private:
    // duration_ : 한 사이클 길이. 음수 / 0 도 입력 가능 (Progress 가 1.0 로
    //             처리해 "항상 ready" 동작이 됨).
    float duration_;

    // remaining_: 현재 남은 시간. Update 가 매 프레임 dt 만큼 감소시킨다.
    //             값이 <= 0 이면 IsReady() == true.
    float remaining_;
};
