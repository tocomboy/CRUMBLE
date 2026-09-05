// =============================================================================
// src/game/weapons/Mortar.cpp — Mortar 구현.
// =============================================================================
//
// 출처:
//   - 메인 플랜 P5 Task 5.4 의 코드 + 본 프로젝트 컨벤션 (한국어 주석) 보강.
// =============================================================================

#include "Mortar.h"

Mortar::Mortar(ProjectilePool& pp, int ownerIdx)
    : pool_(pp),
      owner_(ownerIdx),
      ammo_(1),                         // 시작 시 탄창 1 발.
      reloadTimer_(kReloadSec)          // 초기 ready (즉시 발사 가능).
{
    // 멤버 초기화 리스트로 충분.
}

void Mortar::Update(float dt) {
    // Rifle / Shotgun 과 동일 패턴 — 재장전 진행 + ready 시 ammo 리필.
    if (!reloadTimer_.IsReady()) {
        reloadTimer_.Update(dt);
        if (reloadTimer_.IsReady()) {
            ammo_ = 1;
        }
    }
}

bool Mortar::TryFire(Vec2 origin, Vec2 /*fireDir*/) {
    // 발사 거부 — 재장전 중 / 탄 0.
    //   - fireDir 인자는 무시: 박격포의 발사 방향은 aim_.CurrentDirection()
    //     이 결정. Player 가 release 시점에 호출하면 그 순간의 sine
    //     oscillation 위상이 그대로 발사 방향.
    if (!reloadTimer_.IsReady()) return false;
    if (ammo_ <= 0) return false;

    // 현재 aim 위상으로 방향 + 속도 결정. 이 두 값이 결합해 발사 직후의
    // 초기 속도 벡터 = direction × speed.
    //   - direction.y < 0 (위쪽) 이라 자연스럽게 포물선 비행 — PhysicsWorld
    //     의 중력이 vy 를 매 step 증가시켜 결국 다시 떨어진다.
    Vec2 dir = aim_.CurrentDirection();
    float spd = aim_.CurrentSpeed();

    // 포탄 spawn — ProjectileKind::Mortar 로 식별되어 P8 의 폭발 처리에서
    // 분기 대상이 된다 (현재 P5 는 직격만, 폭발 없음).
    //   - radius 6 px → AABB half=(6,6) (Projectile.cpp 의 결함 #7 fix 정합).
    //   - lifetime 5s → 화면 끝까지 비행해도 안전.
    //   - damage 35 → 직격 시 HP 65 이하 → 거의 즉사 위험. 명중 난이도가
    //     높아 (조준 + 사인파 정확 발사) 균형.
    pool_.Spawn(ProjectileKind::Mortar, owner_, origin, dir * spd,
                kDirectDmg, kLifetime, kRadius);

    --ammo_;
    if (ammo_ == 0) {
        reloadTimer_.Start();
    }
    // 발사 후 aim_ 초기화 — 다음 EnterMortarAiming 진입 시 phase 0 부터
    // 다시 시작.
    aim_.Reset();
    return true;
}

void Mortar::OnSwitchedOut() {
    // 다른 무기로 전환 시 aim phase 초기화. 사용자가 박격포 조준 도중 무기
    // 전환하면 진행 중이던 게이지 / 각도가 사라진다 (의도된 동작 — 박격포
    // 쪽 누적 위상이 다음 진입 시 잔존하지 않게).
    aim_.Reset();
}
