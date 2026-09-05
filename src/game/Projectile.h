// =============================================================================
// src/game/Projectile.h — 투사체 데이터 + 풀 관리자.
// =============================================================================
//
// 목적:
//   - 무기 (Rifle / Shotgun / Mortar) 가 발사한 한 발의 정보를 기술.
//   - PhysicsWorld 의 dynamic body 와 1:1 대응 — userData 에 Projectile* 를
//     심어 충돌 콜백에서 빠르게 식별 가능.
//   - ProjectilePool 은 capacity 100 의 일반 풀 (Pool<Projectile>) 위에서
//     수명 관리 (lifetime 만료 / expired 플래그) 와 PhysicsBody 동기 생성/파괴
//     를 담당.
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 3 Task 3.2.
//   - 풀 패턴은 src/core/Pool.h (P3.1 에서 도입).
//   - PhysicsWorld::CreateBody / DestroyBody / SetContactBeginCallback 는
//     P1 단계 (PhysicsWorld.h) 에서 이미 제공.
//   - Plan Revisability 노트: Week 11 (Physics Engine 1) 에 투사체 라이프사이클
//     (CCD/sleep/sweep) 패턴이 등장하면 본 클래스의 Update/Despawn 을 그
//     패턴으로 정렬한다.
//
// 결정론 / 스레드:
//   - 모든 mutation 은 메인 루프 (메인 스레드) 에서 호출되는 Update / Spawn
//     / Despawn 을 통해서만 발생.
//   - PhysicsWorld 와 같은 스레드 단일성 가정.
//
// expired 플래그 분리 이유:
//   - 충돌 콜백 안에서 Despawn 을 즉시 호출하면 PhysicsWorld 의 contact 순회
//     도중 body 를 파괴하게 되어 안전하지 않다 (벡터 인덱스 무효화 위험).
//   - 따라서 콜백은 expired = true 만 마킹하고, ProjectilePool::Update 의
//     ForEachAlive 루프가 다음 frame 시작에 안전한 시점에 실제 파괴/재활용을
//     수행 ("deferred destruction" 패턴).
// =============================================================================

#pragma once

#include "../core/Pool.h"
#include "../math/Vec2.h"
#include "../physics/PhysicsWorld.h"

// ProjectileKind: 투사체 종류 분류.
//   - Rifle         : 직사 + knockback 약 (P3 MVP).
//   - ShotgunPellet : Shotgun 한 펠릿 (P5 에서 사용).
//   - Mortar        : 박격포 포탄 — 폭발 시 청크 생성 (P5/P8 에서 사용).
// 본 P3 단계에서는 Rifle 만 실제로 발사되지만, enum 은 미리 정의해 후속
// 무기 추가 시 재컴파일 부담을 줄인다.
enum class ProjectileKind {
    Rifle,
    ShotgunPellet,
    Mortar
};

// Projectile: 한 발의 상태.
//   - bodyId            : PhysicsWorld 가 발급한 dynamic body 의 인덱스.
//   - lifetime / max    : 누적 시간 / 자동 만료 시한 (초). 만료 시 풀 반환.
//   - damage            : 충돌 시 victim HP 에서 차감할 양.
//   - ownerPlayerIndex  : 0 = P1, 1 = P2. 자기 자신이 쏜 탄에 맞지 않게.
//   - kind              : 충돌 콜백이 무기별 분기를 할 때 검사.
//   - expired           : true 면 다음 Update 에서 풀 반환 (deferred destruction).
struct Projectile {
    BodyId         bodyId           = -1;
    float          lifetime         = 0.0f;
    float          maxLifetime      = 1.5f;
    int            damage           = 12;
    int            ownerPlayerIndex = -1;
    ProjectileKind kind             = ProjectileKind::Rifle;
    bool           expired          = false;
};

// ProjectilePool: 풀 + PhysicsWorld 동기 관리자.
//
// 사용 예 (메인 루프 일부):
//   ProjectilePool pp;
//   pp.Init(world, 100);
//   ...
//   pp.Spawn(ProjectileKind::Rifle, 0, p1.pos, dir * 1200, 12, 1.5f, 3.0f);
//   ...
//   pp.Update(dt);   // 매 fixed-step 에 한 번.
class ProjectilePool {
public:
    // Init: world 참조 저장 + Pool<Projectile> 용량 확보.
    //   - world 의 수명이 ProjectilePool 보다 길어야 함 (포인터 보관).
    void Init(PhysicsWorld& world, int capacity);

    // Spawn: 풀에서 한 슬롯을 잡고 PhysicsWorld 에 dynamic body 를 만든다.
    //   - origin   : 발사 지점 (예: 플레이어 중앙).
    //   - velocity : 초기 속도 벡터 (방향 * 속도).
    //   - damage   : victim HP 에 차감할 양.
    //   - radius   : Circle shape 반지름 (픽셀).
    //   - 반환     : projectile id (>=0) 또는 풀이 가득차면 -1.
    int  Spawn(ProjectileKind kind,
               int  ownerPlayerIndex,
               Vec2 origin,
               Vec2 velocity,
               int  damage,
               float maxLifetime,
               float radius);

    // Update: dt 만큼 lifetime 누적 + 만료/expired 슬롯 정리.
    //   - PhysicsWorld::DestroyBody 를 호출하므로 step 사이에서 호출해야
    //     contact 콜백 도중 무효화 위험이 없다.
    void Update(float dt);

    // Despawn: 외부 (충돌 콜백 등) 에서 "이 탄 끝내" 요청.
    //   - 즉시 파괴하지 않고 expired 만 true. 다음 Update 가 정리.
    void Despawn(int projectileId);

    // 외부 (테스트, HUD, 충돌 콜백) 가 풀 상태를 볼 수 있게 reference 노출.
    Pool<Projectile>& Storage() { return pool_; }
    PhysicsWorld&     World()   { return *world_; }

private:
    PhysicsWorld*    world_ = nullptr;   // 외부 소유. 생명주기 책임 없음.
    Pool<Projectile> pool_;
};
