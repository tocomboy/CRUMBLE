// =============================================================================
// src/game/weapons/Shotgun.h — 산탄총 (P5).
// =============================================================================
//
// 무기 사양 (메인 스펙 §5 + 플랜 P5.2):
//   - 탄창 크기      2 발
//   - 한 발 당 펠릿  6 개
//   - 스프레드       ±15° (kSpread)
//   - 재장전 시간    1.5 초
//   - 펠릿 속도      800 픽셀/초 (±50 jitter)
//   - 펠릿 데미지    4 (6펠릿 × 4 = 최대 24 / shot — 근거리에 강력)
//   - 펠릿 수명      0.4 초 (짧음 — 산탄은 근거리 무기)
//   - 펠릿 반지름    2 픽셀 (Rifle 보다 작음)
//   - Recoil         0.6 단위 충격량 (반대 방향) — Player body 에 즉시 ApplyImpulse
//
// 동작 사이클:
//   1) ammo_ = 2 / reloadTimer_ ready 로 시작.
//   2) TryFire 호출 → 6 발 spawn → ammo_-- → recoil 계산.
//   3) ammo_ == 0 시 reloadTimer_.Start().
//   4) Update(dt) 에서 reloadTimer_ 진행. ready 되면 ammo_ = 2 리필.
//
// 차별점 (vs Rifle):
//   - 한 호출에 ProjectilePool 을 6 번 소비 — 풀 capacity (100) 안에 충분히 여유.
//   - 무작위 spread 로 deterministic 보장 안 됨 (단일 시드 thread_local mt19937).
//     P4 의 결정론 replay 테스트는 PhysicsWorld byte-equal 만 검증하므로 무관.
//   - LastRecoilImpulse 는 main.cpp 의 발사 분기가 즉시 ApplyImpulse 로 받아
//     플레이어를 뒤로 밀어냄 (Differentiated Knockback — P5 Task 5.6).
//
// 출처:
//   - 메인 플랜 P5 Task 5.2 의 코드 + 한국어 주석 보강.
//   - kPi 는 src/math/Vec2.h 의 constexpr 상수.
// =============================================================================

#pragma once

#include "../../core/CooldownTimer.h"
#include "../../math/Vec2.h"
#include "../Projectile.h"
#include "../Weapon.h"

class Shotgun : public Weapon {
public:
    // 생성자 인자:
    //   - pp       : 투사체 풀 reference (외부 소유).
    //   - ownerIdx : 0 = P1, 1 = P2 — friendly fire 차단용.
    Shotgun(ProjectilePool& pp, int ownerIdx);

    // -- Weapon 인터페이스 구현 --
    void Update(float dt) override;
    bool TryFire(Vec2 origin, Vec2 fireDir) override;

    int   AmmoLeft()       const override { return ammo_; }
    int   MagSize()        const override { return kMagSize; }
    float ReloadProgress() const override { return reloadTimer_.Progress(); }
    bool  IsReloading()    const override { return !reloadTimer_.IsReady(); }
    const char* Name()     const override { return "Shotgun"; }

    // 가장 최근 TryFire 가 만든 recoil 충격량 (반대 방향 벡터).
    //   - 발사 실패 (재장전 / 0 벡터) 시 마지막 성공 값이 그대로 남음 — 호출자
    //     가 TryFire 의 반환값으로 분기해 ApplyImpulse 여부를 결정해야 한다.
    //   - main.cpp 의 발사 분기 (P5 Task 5.6) 가 본 값을 Player body 에 즉시
    //     ApplyImpulse.
    Vec2 LastRecoilImpulse() const { return lastRecoil_; }

private:
    ProjectilePool& pool_;          // 외부 소유.
    int             owner_;
    int             ammo_;
    Vec2            lastRecoil_{};  // 가장 최근 발사의 recoil (반대 방향).
    CooldownTimer   reloadTimer_;
    CooldownTimer   fireTimer_;     // 발사 간 cooldown (펌프 액션 느낌).

    // -- 무기 파라미터 (P5 고정 값) --
    static constexpr int   kMagSize   = 2;
    static constexpr int   kPellets   = 6;
    static constexpr float kSpread    = 15.0f * kPi / 180.0f;   // ±15°.
    static constexpr float kReloadSec = 1.5f;
    // kFireSec: 발사 간 최소 간격. 펌프 액션 산탄총의 무게감 — 0.5초 → 한 번에
    //   2 발 다 빠지지 않고 시각적으로 명확한 간격. mag 2 발 → 0.5초 후 두
    //   번째 → 1.5초 reload → 다시 2 발.
    //   사용자 시각 검증 후 추가 (P5 결함 보강).
    static constexpr float kFireSec   = 0.5f;
    static constexpr float kSpeed     = 800.0f;
    static constexpr int   kDamage    = 4;
    static constexpr float kLifetime  = 0.4f;
    static constexpr float kRadius    = 2.0f;
    static constexpr float kRecoilImp = 0.6f;
};
