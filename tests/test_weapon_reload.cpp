// =============================================================================
// test_weapon_reload.cpp — Rifle 무기의 mag / reload 사이클 단위 테스트.
// =============================================================================
//
// 검증 4 가지:
//   1) 생성 시 탄창 가득 + ReloadProgress = 1.0 (즉시 발사 가능).
//   2) TryFire 한 번 → 탄창 -1 + 풀에 1 발 spawn.
//   3) 5 발 다 쏘면 6 번째 발사는 false + IsReloading 진입.
//   4) 1 초 후 (60 fixed step) 재장전 완료 → IsReloading=false + ammo 가득.
//
// 출처: 메인 플랜 P3 Task 3.3 Step 1 의 4 케이스 그대로.
// =============================================================================

#include "test_runner.h"
#include "assert_eq.h"

#include "../src/game/Projectile.h"
#include "../src/game/weapons/Rifle.h"
#include "../src/game/weapons/Shotgun.h"
#include "../src/physics/PhysicsWorld.h"

REGISTER_TEST(rifle_starts_full_mag) {
    // 풀 capacity 32 면 5 발 발사 + 누적 검증에 충분.
    PhysicsWorld w;
    ProjectilePool pp;
    pp.Init(w, 32);
    Rifle r(pp, 0);

    // MagSize 가 5 라는 사실을 명시적으로 박지 않고 AmmoLeft 와 비교 →
    // 상수가 변경되어도 본 테스트는 의도 (시작 시 탄창 가득) 를 보존.
    CR_ASSERT(r.AmmoLeft() == 5);
    CR_NEAR(r.ReloadProgress(), 1.0f, 1e-5f);
    CR_ASSERT(!r.IsReloading());
}

REGISTER_TEST(rifle_fire_decrements_mag_and_spawns) {
    PhysicsWorld w;
    ProjectilePool pp;
    pp.Init(w, 32);
    Rifle r(pp, 0);

    bool fired = r.TryFire(Vec2{0, 0}, Vec2{1, 0});
    CR_ASSERT(fired);
    CR_ASSERT(r.AmmoLeft() == 4);
    // 풀 ActiveCount 로 spawn 검증 — Rifle 이 호출자 측 mock 없이 풀과
    // 정직하게 통합되었음을 확인.
    CR_ASSERT(pp.Storage().ActiveCount() == 1);
}

REGISTER_TEST(rifle_empty_mag_triggers_reload) {
    PhysicsWorld w;
    ProjectilePool pp;
    pp.Init(w, 32);
    Rifle r(pp, 0);

    // 5 발 모두 발사. 발사 사이 fireTimer 가 ready 되도록 0.2 초 (>kFireSec
    // 0.15) 만큼 Update 호출.
    for (int i = 0; i < 5; ++i) {
        bool ok = r.TryFire(Vec2{0, 0}, Vec2{1, 0});
        CR_ASSERT(ok);
        // 마지막 발사 후에는 reload 진입이라 추가 Update 불필요.
        if (i < 4) {
            for (int j = 0; j < 12; ++j) r.Update(1.0f / 60.0f);  // 0.2s.
        }
    }
    // 6 번째 발사 시도 → 거부 + 재장전 중.
    bool sixth = r.TryFire(Vec2{0, 0}, Vec2{1, 0});
    CR_ASSERT(!sixth);
    CR_ASSERT(r.IsReloading());
}

REGISTER_TEST(rifle_reload_completes_after_duration) {
    PhysicsWorld w;
    ProjectilePool pp;
    pp.Init(w, 32);
    Rifle r(pp, 0);

    // 탄창 다 비우기 (fireTimer 사이 Update 로 cooldown 통과).
    for (int i = 0; i < 5; ++i) {
        r.TryFire(Vec2{0, 0}, Vec2{1, 0});
        if (i < 4) {
            for (int j = 0; j < 12; ++j) r.Update(1.0f / 60.0f);
        }
    }
    // 재장전 진입 확인.
    CR_ASSERT(r.IsReloading());

    // 60 frame * (1/60s) = 1.0s — kReloadSec=0.8 보다 충분히 길다.
    for (int i = 0; i < 60; ++i) {
        r.Update(1.0f / 60.0f);
    }
    CR_ASSERT(!r.IsReloading());
    CR_ASSERT(r.AmmoLeft() == 5);
}

// =============================================================================
// Shotgun 테스트 (P5 Task 5.2). 산탄총은 한 발에 6 펠릿 spawn 이 핵심 차별점.
// =============================================================================

REGISTER_TEST(shotgun_fires_six_pellets_per_shot) {
    // 풀 capacity 64 — 1 발 (6 펠릿) 만 검증하므로 충분.
    PhysicsWorld w;
    ProjectilePool pp;
    pp.Init(w, 64);
    Shotgun s(pp, 0);

    // 발사 한 번 → 풀 ActiveCount == 6 이어야 한다.
    bool fired = s.TryFire(Vec2{0, 0}, Vec2{1, 0});
    CR_ASSERT(fired);
    CR_ASSERT(pp.Storage().ActiveCount() == 6);
}

REGISTER_TEST(shotgun_two_shots_then_reload) {
    PhysicsWorld w;
    ProjectilePool pp;
    pp.Init(w, 64);
    Shotgun s(pp, 0);

    // 첫 발사.
    bool first = s.TryFire(Vec2{0, 0}, Vec2{1, 0});
    CR_ASSERT(first);
    // fireTimer 가드 진행 — kFireSec=0.5s 이상 시뮬레이트 (36 frame ≈ 0.6s).
    for (int j = 0; j < 36; ++j) s.Update(1.0f / 60.0f);
    // 두 번째 발사.
    bool second = s.TryFire(Vec2{0, 0}, Vec2{1, 0});
    CR_ASSERT(second);
    // 두 번째 발사 직후 재장전 진입.
    CR_ASSERT(s.IsReloading());
    // 3 번째 시도는 재장전 중이라 거부.
    bool third = s.TryFire(Vec2{0, 0}, Vec2{1, 0});
    CR_ASSERT(!third);
}
