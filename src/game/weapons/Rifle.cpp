// =============================================================================
// src/game/weapons/Rifle.cpp — Rifle 구현.
// =============================================================================
//
// 출처:
//   - 메인 플랜 P3 Task 3.3 Step 4 의 코드 + 한국어 주석 보강.
//   - 발사 직선 산식: dir = fireDir.Normalized() ; vel = dir * kSpeed.
//     현재 P3 단계는 발사 방향이 단순 좌/우 (플레이어 속도 부호) 로 결정 —
//     main.cpp 가 Vec2{1,0} / Vec2{-1,0} 을 그대로 넘긴다.
// =============================================================================

#include "Rifle.h"

Rifle::Rifle(ProjectilePool& pp, int ownerIdx)
    : pool_(pp),
      owner_(ownerIdx),
      ammo_(kMagSize),                // 시작 시 탄창 가득.
      reloadTimer_(kReloadSec),       // 초기 remaining=0 → 즉시 IsReady.
      fireTimer_(kFireSec)            // 발사 간 cooldown — 초기 ready.
{
    // 멤버 초기화 리스트로 충분. 본문 비움.
}

void Rifle::Update(float dt) {
    // 재장전 타이머: 진행 + ready 시 mag 리필.
    if (!reloadTimer_.IsReady()) {
        reloadTimer_.Update(dt);
        if (reloadTimer_.IsReady()) {
            ammo_ = kMagSize;
        }
    }
    // 발사 cooldown: hold 자동연사 fire-rate 상한. 매 frame 진행해야 다음
    // 발사가 정확히 kFireSec 후에 ready.
    if (!fireTimer_.IsReady()) {
        fireTimer_.Update(dt);
    }
}

bool Rifle::TryFire(Vec2 origin, Vec2 fireDir) {
    // 발사 거부 케이스 1: 재장전 중.
    if (!reloadTimer_.IsReady()) return false;
    // 발사 거부 케이스 2: 발사 cooldown 중 (P5 추가 — fire-rate 상한).
    //   사용자 시각 검증에서 "기본 연사속도가 너무 빠르다" 피드백 → 본
    //   가드가 hold 자동연사의 max rate 를 1/kFireSec 로 제한.
    if (!fireTimer_.IsReady())   return false;
    // 발사 거부 케이스 3: 탄창 비어있음 (정상 흐름이라면 아래 ammo_==0 분기에서
    // 재장전이 시작됐어야 하므로 여기 도달은 일시적 race 또는 외부 코드의
    // ammo 강제 0 설정 같은 비정상). 안전망으로 false.
    if (ammo_ <= 0) return false;
    // 발사 거부 케이스 4: 0 벡터.
    Vec2 dir = fireDir.Normalized();
    if (dir.LengthSq() < 1e-6f) return false;

    // 풀에 spawn 위임.
    pool_.Spawn(ProjectileKind::Rifle, owner_, origin, dir * kSpeed,
                kDamage, kLifetime, kRadius);
    --ammo_;

    // 발사 cooldown 시작 — 다음 TryFire 는 kFireSec 후에 가능.
    fireTimer_.Start();

    // 마지막 탄을 쏘면 재장전 시작.
    if (ammo_ == 0) {
        reloadTimer_.Start();
    }
    return true;
}
