// =============================================================================
// src/game/Projectile.cpp — Projectile / ProjectilePool 구현.
// =============================================================================
//
// 출처:
//   - 메인 플랜 P3 Task 3.2 Step 2 의 코드 스니펫을 기반으로, 프로젝트
//     코딩 컨벤션 (한국어 주석 + PDF 출처 표기 의무) 에 맞춰 보강.
//   - PhysicsBody 필드 의미는 src/physics/PhysicsBody.h 를 참조.
// =============================================================================

#include "Projectile.h"

#include "../physics/PhysicsBody.h"

void ProjectilePool::Init(PhysicsWorld& world, int capacity) {
    // world 는 외부 객체 — 포인터로 잡고 수명은 호출자가 관리.
    // capacity 는 게임 시작 시 한 번 잡은 후 변경하지 않는다 (Pool::Init 의
    // 동적 resize 미지원 정책에 따름).
    world_ = &world;
    pool_.Init(capacity);
}

int ProjectilePool::Spawn(ProjectileKind kind,
                          int  ownerPlayerIndex,
                          Vec2 origin,
                          Vec2 velocity,
                          int  damage,
                          float maxLifetime,
                          float radius) {
    // 1) Pool 에서 빈 슬롯 잡기. 풀 가득차면 -1 반환 (호출자 = Rifle::TryFire
    //    등이 false 처리).
    int id = pool_.Acquire();
    if (id < 0) return -1;

    // 2) Projectile 메타 데이터 초기화. Pool::Acquire 가 storage_[id] = T{} 로
    //    이미 default 화 했으나, 명시적으로 모든 필드를 채워 호출자 의도를
    //    코드에 박아둔다 (가독성 + 회귀 방지).
    Projectile* pj = pool_.Get(id);
    pj->kind             = kind;
    pj->ownerPlayerIndex = ownerPlayerIndex;
    pj->lifetime         = 0.0f;
    pj->maxLifetime      = maxLifetime;
    pj->damage           = damage;
    pj->expired          = false;

    // 3) PhysicsBody 정의. AABB 작은 직사각형 (half=(r,r)) 으로 표현.
    //
    // [P3 결함 fix #7 — narrow-phase 정렬]
    //   원안: ShapeType::Circle. 그러나 Collision.cpp 의 DetectAABBCircle /
    //   DetectCircle 은 P4 에서 활성화 예정 stub 이라 항상 false 반환 →
    //   투사체가 어떤 body 와도 충돌하지 않아 데미지가 발생하지 않음.
    //   본 P3 단계에서는 AABB shape 으로 통일해 narrow-phase 를 P4 의
    //   작업 영역 침범 없이 유지. 시각적으로도 "직사각형 투사체" 가 메인
    //   스펙 §5 (Rifle 시각 명세) 와 일치한다.
    //   P4 에서 DetectAABBCircle / DetectCircle 가 활성화되면 본 코드를
    //   ShapeType::Circle 로 되돌릴지는 그 시점에서 재검토 (Plan
    //   Revisability Policy) — 본 P3 의 AABB 표현은 root-cause 수정이며
    //   워크어라운드가 아니다.
    PhysicsBody def;
    def.pos         = origin;
    def.vel         = velocity;
    def.shape       = ShapeType::AABB;
    // half = (r, r) — 작은 정사각형. 시각적으로 Rifle 탄을 점에 가깝게
    // 표현하고 narrow-phase 의 AABB-AABB 분기 (P1 에서 이미 활성) 를 사용.
    def.half        = {radius, radius};
    // mass=0.05kg → invMass=20. 가벼워서 충돌 시 큰 힘을 받으면 즉시 튕기지만,
    // P3 의 직사 투사체는 충돌 후 expired 플래그로 즉시 사라지므로 영향 미미.
    def.SetMass(0.05f);
    // restitution=0 / friction=0: 직사 투사체는 튕김도 마찰도 없음. 순수 직선
    // 운동 후 만료 또는 충돌 → expired.
    def.restitution = 0.0f;
    def.friction    = 0.0f;
    def.type        = BodyType::Dynamic;
    // userData 에 Projectile* 를 심어 충돌 콜백에서 static_cast 로 즉시 식별.
    // void* 인 이유는 PhysicsBody.h 주석 참조 (물리 코어 ↔ 게임 타입 의존성
    // 차단).
    def.userData    = pj;

    // P8 collision layer wiring:
    //   layer = 1<<1 (Projectile). mask 는 Player + Chunk + Static + Platform.
    //   - Projectile vs Projectile 은 mask 에서 bit 1 제외 → 자동 무시 (결함
    //     #10 의 펠릿 self-collision 도 본 mask 로 자연 차단되지만, P5 의
    //     CombatHooks early-return 분기는 안전망으로 유지).
    def.collisionLayer = 1u << 1;
    def.collisionMask  = (1u << 0) | (1u << 2) | (1u << 3) | (1u << 4);

    // 4) PhysicsWorld 에 등록 → BodyId 받음.
    pj->bodyId = world_->CreateBody(def);
    return id;
}

void ProjectilePool::Update(float dt) {
    // ForEachAlive 안에서 같은 슬롯을 Release 해도 안전 (Pool.h ForEachAlive
    // 주석 참조). 자기 자신을 release 후 인덱스 i 가 다음으로 진행하므로
    // 무한 루프 위험 없음.
    pool_.ForEachAlive([&](int id, Projectile& p) {
        p.lifetime += dt;
        // 만료 조건:
        //   - lifetime >= maxLifetime : 자연 수명 만료 (예: Rifle 1.5s)
        //   - expired                 : 외부 (충돌 콜백) 가 deferred 파괴 요청
        if (p.lifetime >= p.maxLifetime || p.expired) {
            // PhysicsWorld 측 body 도 함께 파괴 — 그렇지 않으면 시뮬레이션에
            // 유령 body 가 남아 다른 충돌을 유발한다.
            world_->DestroyBody(p.bodyId);
            pool_.Release(id);
        }
    });
}

void ProjectilePool::Despawn(int id) {
    Projectile* p = pool_.Get(id);
    // 이미 죽었거나 invalid id 면 조용히 무시 (double-despawn 방어).
    if (!p) return;
    // 즉시 파괴하지 않고 expired 만 true. 다음 Update() 가 안전한 시점에
    // PhysicsWorld::DestroyBody + Pool::Release 를 묶어 처리.
    p->expired = true;
}
