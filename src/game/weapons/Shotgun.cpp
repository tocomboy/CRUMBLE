// =============================================================================
// src/game/weapons/Shotgun.cpp — Shotgun 구현.
// =============================================================================
//
// 출처:
//   - 메인 플랜 P5 Task 5.2 의 코드 스니펫 + 본 프로젝트 컨벤션 (한국어 주석)
//     으로 보강.
// =============================================================================

#include "Shotgun.h"

#include <cmath>
#include <random>

Shotgun::Shotgun(ProjectilePool& pp, int ownerIdx)
    : pool_(pp),
      owner_(ownerIdx),
      ammo_(kMagSize),                // 시작 시 탄창 가득 (2 발).
      reloadTimer_(kReloadSec),       // 초기 remaining=0 → 즉시 IsReady.
      fireTimer_(kFireSec)            // 발사 간 cooldown — 초기 ready.
{
    // 멤버 초기화 리스트로 충분.
}

void Shotgun::Update(float dt) {
    // Rifle::Update 와 동일 패턴 — 재장전 + 발사 cooldown 진행.
    if (!reloadTimer_.IsReady()) {
        reloadTimer_.Update(dt);
        if (reloadTimer_.IsReady()) {
            ammo_ = kMagSize;
        }
    }
    if (!fireTimer_.IsReady()) {
        fireTimer_.Update(dt);
    }
}

bool Shotgun::TryFire(Vec2 origin, Vec2 fireDir) {
    // 발사 거부 케이스 — Rifle 과 동일 + fireTimer 가드.
    if (!reloadTimer_.IsReady()) return false;
    if (!fireTimer_.IsReady())   return false;
    if (ammo_ <= 0) return false;
    Vec2 dir = fireDir.Normalized();
    if (dir.LengthSq() < 1e-6f) return false;

    // RNG: thread_local fixed-seed mt19937. 게임은 메인 스레드 단독이므로
    // 사실상 전역과 동일 동작. 시드 12345 는 매 빌드에서 같은 첫 펠릿 산포.
    //   - 결정론 replay 테스트 (P4) 는 PhysicsWorld 만 검증해 본 무작위가
    //     영향 없음.
    //   - 발표 직전 튜닝 시 시드 변경하면 산포 패턴이 달라진다.
    static thread_local std::mt19937 rng(12345u);
    std::uniform_real_distribution<float> spreadDist(-kSpread, kSpread);
    std::uniform_real_distribution<float> speedJit(-50.0f, 50.0f);

    // 6 발의 펠릿을 base 방향 ± 스프레드 안에서 spawn.
    //   - atan2 로 base 각도 → 각 펠릿 각도에 spreadDist 더해 cos/sin 으로
    //     단위 벡터 → 속도 = (kSpeed + jitter).
    //   - speedJit 로 펠릿마다 약간 다른 속도 → 시각적으로 자연스러운 cone.
    float baseAngle = std::atan2(dir.y, dir.x);
    for (int i = 0; i < kPellets; ++i) {
        float a = baseAngle + spreadDist(rng);
        float s = kSpeed + speedJit(rng);
        Vec2 v{std::cos(a) * s, std::sin(a) * s};
        pool_.Spawn(ProjectileKind::ShotgunPellet, owner_, origin, v,
                    kDamage, kLifetime, kRadius);
    }

    --ammo_;
    // 발사 cooldown 시작 — 다음 TryFire 는 kFireSec 후에 가능.
    fireTimer_.Start();
    if (ammo_ == 0) {
        reloadTimer_.Start();
    }

    // Recoil: dir 의 반대 방향 × 임펄스 크기.
    //   - main.cpp 의 발사 분기가 TryFire 가 true 면 즉시 LastRecoilImpulse()
    //     를 가져와 ApplyImpulse — 슈터를 뒤로 밀어냄.
    lastRecoil_ = dir * (-kRecoilImp);
    return true;
}
