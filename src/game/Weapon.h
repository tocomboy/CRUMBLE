// =============================================================================
// src/game/Weapon.h — 모든 무기의 추상 베이스.
// =============================================================================
//
// 목적:
//   - Rifle / Shotgun / Mortar 가 공유하는 인터페이스 정의.
//   - 게임 측 (main.cpp 의 발사 분기, HUD 의 reload 게이지) 가 구체 무기 타입
//     을 몰라도 동작하도록 가상 함수로 dispatch.
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 3 Task 3.3.
//   - 무기 파라미터 (mag/reload/속도/dmg) 는 메인 스펙 §5 (Weapons) 와
//     플랜 Phase 3 의 Rifle 상수 (kMagSize=5, kReloadSec=0.8, kSpeed=1200,
//     kDamage=12) 가 단일 정전.
//   - Plan Revisability: Week 11~12 PDF 가 무기/객체 모델을 다른 추상화
//     (예: ECS component) 로 가르치면 본 인터페이스를 그쪽으로 정렬.
//
// 결정론:
//   - Update / TryFire 는 메인 스레드에서만 호출. dt 는 fixed-step.
// =============================================================================

#pragma once

#include "../math/Vec2.h"

class Weapon {
public:
    // virtual 소멸자 — base 포인터로 derived 객체 delete 가능하게.
    // 본 P3 단계에서는 stack-allocated 한 Rifle 두 개를 직접 들고 있으나,
    // 후속 phase 에서 Player 가 unique_ptr<Weapon> 슬롯으로 무기를 교체
    // (P5/P6 무기 전환) 할 가능성에 대비해 virtual.
    virtual ~Weapon() = default;

    // 매 fixed-step 호출. 쿨다운 / 재장전 진행 + 무기 고유 상태 갱신.
    virtual void Update(float dt) = 0;

    // 발사 시도. 발사 성공하면 true.
    //   - origin  : 발사 지점 (월드 좌표).
    //   - fireDir : 방향 벡터 (정규화 안 되어 있어도 내부에서 처리).
    //   - false 반환 케이스: 재장전 중 / 탄창 0 / fireDir 0 벡터.
    virtual bool TryFire(Vec2 origin, Vec2 fireDir) = 0;

    // 무기 전환 시 호출되는 후크. 예: Mortar 조준 모드에서 다른 무기로
    // 전환하면 조준 게이지 초기화. 기본 구현은 noop.
    virtual void OnSwitchedOut() {}

    // HUD / 디버그 표시용 상태 조회.
    virtual int   AmmoLeft()       const = 0;
    virtual int   MagSize()        const = 0;
    virtual float ReloadProgress() const = 0;
    virtual bool  IsReloading()    const = 0;

    // 무기 이름 (HUD 에 표시. P3 는 "Rifle" 만 노출).
    virtual const char* Name() const = 0;
};
