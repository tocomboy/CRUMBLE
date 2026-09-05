// =============================================================================
// src/game/weapons/Rifle.h — P3 MVP 의 유일한 무기.
// =============================================================================
//
// 무기 사양 (메인 스펙 §5 + 플랜 P3.3):
//   - 탄창 크기    5 발
//   - 재장전 시간  0.8 초
//   - 발사 속도    1200 픽셀/초
//   - 데미지       12 (HP 100 기준 9 발에 사망)
//   - 탄 수명      1.5 초
//   - 탄 반지름    3 픽셀
//   - knockback   약 (P3 단계는 명시적 knockback impulse 없음 — 충돌 시
//                  PhysicsBody 의 일반 충돌 응답으로 대체).
//
// 동작 사이클:
//   1) 생성 시 ammo_ = kMagSize, reloadTimer_ ready (= 즉시 발사 가능).
//   2) TryFire 호출 → ammo_-- → ProjectilePool.Spawn → return true.
//   3) ammo_ == 0 이 된 시점에 reloadTimer_.Start() — 재장전 진입.
//   4) 재장전 중 TryFire → false.
//   5) Update(dt) 에서 reloadTimer_ 진행. IsReady 가 되는 순간 ammo_ 리필.
//
// 출처:
//   - 메인 플랜 P3 Task 3.3 의 Rifle 헤더 코드 + 한국어 주석 보강.
//   - CooldownTimer 인터페이스는 src/core/CooldownTimer.h ([3. Game Loop.pdf
//     p.9]).
// =============================================================================

#pragma once

#include "../../core/CooldownTimer.h"
#include "../Projectile.h"
#include "../Weapon.h"

class Rifle : public Weapon {
public:
    // 생성자 인자:
    //   - pp       : 투사체 풀 reference. Rifle 은 풀의 수명을 책임지지 않는다.
    //   - ownerIdx : 0 = P1, 1 = P2. 자기 자신이 쏜 탄에 맞지 않게 콜백에서
    //                필터링하는 데 사용 (Projectile.ownerPlayerIndex 로 전파).
    Rifle(ProjectilePool& pp, int ownerIdx);

    // -- Weapon 인터페이스 구현 --
    void Update(float dt) override;
    bool TryFire(Vec2 origin, Vec2 fireDir) override;

    // 인라인 게터들 — 호출 비용 0, HUD / 테스트가 빈번히 호출.
    int   AmmoLeft()       const override { return ammo_; }
    int   MagSize()        const override { return kMagSize; }
    float ReloadProgress() const override { return reloadTimer_.Progress(); }
    bool  IsReloading()    const override { return !reloadTimer_.IsReady(); }
    const char* Name()     const override { return "Rifle"; }

private:
    ProjectilePool& pool_;          // 외부 소유. 본 객체보다 길게 살아야 함.
    int             owner_;         // 0 = P1, 1 = P2.
    int             ammo_;          // 현재 탄창 잔량.
    CooldownTimer   reloadTimer_;   // 재장전 타이머. duration = kReloadSec.
    CooldownTimer   fireTimer_;     // 발사 간 쿨다운. duration = kFireSec.

    // -- 무기 파라미터 (P3 MVP 고정 값) --
    // 본 값들은 발표 직전 튜닝 시 한 번에 바꿀 수 있도록 한 곳에 모음.
    // P10 의 F8 디버그 슬라이더 (debug build only) 에서 런타임 변경 후 본
    // 값들로 commit 하는 흐름.
    static constexpr int   kMagSize    = 5;
    static constexpr float kReloadSec  = 0.8f;
    // kFireSec: 발사 간 최소 간격 (초). hold 자동 연사 시 본 값이 fire-rate
    //   상한 — 1/0.15 ≈ 6.7 발/초. 5 발 mag 을 약 0.6 초에 비우는 페이스라
    //   사용자 체감상 "연발 라이플" 의 표준 속도.
    //   Plan 에는 본 상수가 명시 안 되어 P5 시각 검증 단계에서 사용자 요청
    //   ("기본 연사속도가 너무 빠르다") 에 따라 도입.
    static constexpr float kFireSec    = 0.15f;
    static constexpr float kSpeed      = 1200.0f;
    static constexpr int   kDamage     = 12;
    static constexpr float kLifetime   = 1.5f;
    static constexpr float kRadius     = 3.0f;
};
