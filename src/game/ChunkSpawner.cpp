// =============================================================================
// src/game/ChunkSpawner.cpp — chunk 풀 + 비산 + lifetime 구현.
// =============================================================================

#include "ChunkSpawner.h"

#include <cmath>
#include <random>

#include "../physics/PhysicsBody.h"

namespace {

// 화면 밖 이탈 검사 boundary — 화면 (1280x720) 양쪽으로 ~300 px 여유.
// 이 영역 밖으로 떨어진 chunk 는 lifetime 만료 전에도 즉시 despawn.
constexpr float kViewMinX = -300.0f;
constexpr float kViewMaxX = 1600.0f;
constexpr float kViewMinY = -300.0f;
constexpr float kViewMaxY = 1200.0f;

}  // namespace

void ChunkSpawner::Init(PhysicsWorld& world, int capacity) {
    world_ = &world;
    pool_.Init(capacity);
}

void ChunkSpawner::SpawnBurst(Vec2 origin, int count) {
    // RNG: thread_local mt19937 — seed 54321. 같은 호출 시퀀스 → 같은 비산 패턴.
    //   - 발표 시연 결정론을 위해 fixed seed. 다양성 원하면 P10 의 F8 슬라이더로
    //     조정 가능 (현재 단계에서는 하드코딩).
    static thread_local std::mt19937 rng(54321u);
    std::uniform_real_distribution<float> ang(0.0f, 6.2831853f);  // 0 ~ 2π.
    std::uniform_real_distribution<float> spd(kBurstSpeedMin, kBurstSpeedMax);
    // lifetime (P8 시각 튜닝 단계 사용자 피드백): 1.5~2.5s → 2.0~3.0s 로 상향.
    //   잔해가 더 오래 남아 폭발의 시각적 잔상 길어짐. 화면 밖 offscreen 검사도
    //   계속 작동하므로 화면 안에 떠다니는 chunk 만 lifetime 따라 자연 정리.
    std::uniform_real_distribution<float> life(2.0f, 3.0f);

    for (int i = 0; i < count; ++i) {
        const int id = pool_.Acquire();
        if (id < 0) return;     // 풀 가득 — 더 이상 spawn 안 함.

        Chunk* k = pool_.Get(id);
        k->lifetime    = 0.0f;
        k->maxLifetime = life(rng);

        // 방향: 0~2π random angle. 속도 (200~400) × 단위 벡터.
        const float a = ang(rng);
        const float s = spd(rng);
        const Vec2  v{std::cos(a) * s, std::sin(a) * s};

        // PhysicsBody 정의.
        //   - mass 0.1, restitution 0.3 (살짝 튕김), friction 0.7 (마찰 강함 →
        //     더미 형성 시 미끄러지지 않음).
        PhysicsBody def;
        def.pos         = origin;
        def.vel         = v;
        def.shape       = ShapeType::AABB;
        def.half        = {kChunkHalf, kChunkHalf};
        def.SetMass(kChunkMass);
        def.restitution = 0.3f;
        def.friction    = 0.7f;
        def.type        = BodyType::Dynamic;
        def.collisionLayer = kLayer;
        def.collisionMask  = kMask;

        k->bodyId = world_->CreateBody(def);
    }
}

void ChunkSpawner::Reset() {
    // 살아있는 chunk 의 body 모두 파괴 + 풀 슬롯 release.
    pool_.ForEachAlive([&](int id, Chunk& k) {
        if (k.bodyId >= 0) world_->DestroyBody(k.bodyId);
        pool_.Release(id);
    });
}

void ChunkSpawner::Update(float dt) {
    pool_.ForEachAlive([&](int id, Chunk& k) {
        k.lifetime += dt;

        bool offscreen = false;
        if (PhysicsBody* b = world_->GetBody(k.bodyId)) {
            if (b->pos.y > kViewMaxY || b->pos.y < kViewMinY ||
                b->pos.x < kViewMinX || b->pos.x > kViewMaxX) {
                offscreen = true;
            }
        }

        if (k.lifetime >= k.maxLifetime || offscreen) {
            // PhysicsWorld 의 body 도 함께 파괴 — Projectile 패턴과 동일.
            world_->DestroyBody(k.bodyId);
            pool_.Release(id);
        }
    });
}
