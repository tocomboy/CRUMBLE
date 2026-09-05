// =============================================================================
// src/game/weapons/Mortar.h — 박격포 (P5 R1 — direct hit only).
// =============================================================================
//
// 무기 사양 (메인 스펙 §5 + 플랜 P5.4):
//   - 탄창 크기      1 발 (한 번 쏘면 즉시 재장전).
//   - 재장전 시간    3.0 초 (가장 긴 — 무거운 무기 패널티).
//   - 직격 데미지    35 (Rifle 12, Shotgun 24 보다 훨씬 큼 — 즉사 직전 1발).
//   - 탄 수명        5.0 초 (포물선 비행이 길게 가도 안전 만료).
//   - 탄 반지름      6 픽셀 (Rifle 3, Shotgun 2 보다 큼 — 큰 포탄).
//
// 본 P5 (R1) 단계에서 빠진 것 (P8 에서 추가):
//   - 폭발 (radius 안 dynamic body 에 임펄스 + 청크 6 개 spawn 분쇄).
//   - 지형 파괴 (TileGrid 의 tile 일정 반경 안에서 destroy).
//   본 P5 의 Mortar 는 "직격 시 35 dmg + expired" 만 — 지형에 맞아도 직격
//   효과 없음 (CombatHooks 가 expired 만 마킹).
//
// 동작 사이클:
//   1) ammo_ = 1 / reloadTimer_ ready 로 시작.
//   2) Player 가 발사 키 누름 → Player::EnterMortarAiming → body 가
//      Kinematic 으로 일시 전환.
//   3) 이 상태에서 Player::UpdateInput 가 fire hold 로 aim_.AdvanceGauge,
//      angle hold 로 aim_.AdvanceAngle.
//   4) Player 가 발사 키 뗌 → Player::TryFireMortar → Mortar::TryFire.
//   5) Mortar::TryFire 가 aim_ 의 현재 방향/속도로 ProjectilePool.Spawn.
//   6) ammo_ = 0 → reloadTimer_.Start() → 3 초 후 재충전.
//   7) Mortar 발사 후 또는 무기 전환 시 aim_.Reset() (phase 0 으로).
//
// Aim() 외부 노출:
//   - 본 클래스가 MortarAimState 를 보유하지만 갱신은 Player::UpdateInput
//     이 담당. 따라서 Aim() 으로 reference 를 노출해 외부가 AdvanceGauge /
//     AdvanceAngle 를 호출.
//   - facingSign 도 Player::SetFacingSign 가 직접 aim_.facingSign 을 갱신.
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 5 Task 5.4.
//   - kReloadSec 등 상수는 메인 스펙 §5 Mortar 섹션의 정전.
// =============================================================================

#pragma once

#include "../../core/CooldownTimer.h"
#include "../../math/Vec2.h"
#include "../Projectile.h"
#include "../Weapon.h"
#include "MortarAimState.h"

class Mortar : public Weapon {
public:
    Mortar(ProjectilePool& pp, int ownerIdx);

    // -- Weapon 인터페이스 구현 --
    void Update(float dt) override;
    bool TryFire(Vec2 origin, Vec2 fireDir) override;
    // 무기 전환 시 호출 — aim_ 상태 초기화 (phase 0).
    void OnSwitchedOut() override;

    int   AmmoLeft()       const override { return ammo_; }
    int   MagSize()        const override { return 1; }
    float ReloadProgress() const override { return reloadTimer_.Progress(); }
    bool  IsReloading()    const override { return !reloadTimer_.IsReady(); }
    const char* Name()     const override { return "Mortar"; }

    // 외부 (Player::UpdateInput / 시각화 코드) 가 aim 상태 갱신/조회.
    //   - non-const reference: AdvanceGauge / AdvanceAngle / Reset 호출 가능.
    //   - const overload 는 시각화 (게이지 바, 방향선) 가 사용.
    MortarAimState&       Aim()       { return aim_; }
    const MortarAimState& Aim() const { return aim_; }

    // -- 무기 파라미터 (P5 R1 고정 값) --
    //   public 으로 두는 이유: Player.cpp 의 TryFireMortar 가 spawn origin 을
    //   player 외곽 + kRadius 만큼 offset 하기 위해 외부에서 접근 (Plan 결함
    //   #14 fix — spawn 직후 owner self-contact 회피).
    static constexpr float kReloadSec = 3.0f;
    static constexpr float kRadius    = 6.0f;
    static constexpr int   kDirectDmg = 35;
    static constexpr float kLifetime  = 5.0f;

private:
    ProjectilePool& pool_;          // 외부 소유.
    int             owner_;
    int             ammo_;
    MortarAimState  aim_;
    CooldownTimer   reloadTimer_;
};
