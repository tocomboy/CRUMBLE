// =============================================================================
// src/game/ChunkSpawner.h — 박격포 폭발 시 비산하는 chunk 풀.
//
// 목적:
//   - TileGrid.DestroyInRadius 의 onDestroyed 콜백이 본 클래스의 SpawnBurst 를
//     호출 → 파괴된 타일 중심에서 6 개 chunk 가 사방으로 비산 (메인 스펙 §3.4).
//   - chunk 는 dynamic body — 중력 + 충돌 응답으로 자연스럽게 떨어진다.
//   - lifetime 만료 또는 화면 밖 이탈 시 자동 despawn.
//
// 충돌 정책 (D2.5 — 메인 스펙 §3.4):
//   - chunk 는 player 를 막아 / 밀어내지만 데미지는 없음 (D3 거부 — 스펙 §7).
//   - chunk 끼리도 충돌해 더미 형성 (시각적 잔해).
//   - projectile 과는 충돌 X (chunk 가 탄에 영향 받으면 chain reaction 비예측).
//
//   collisionLayer / Mask 비트 (P8 Task 8.2 wiring):
//     bit 0 = Player, bit 1 = Projectile, bit 2 = Chunk, bit 3 = Static (terrain),
//     bit 4 = MovingPlatform.
//   - Chunk layer = 1<<2.
//   - Chunk mask  = (1<<0)|(1<<2)|(1<<3)|(1<<4) — Player + Chunk + Static + Platform.
//
// 결정론:
//   - thread_local mt19937 (seed 54321) — 같은 입력에 같은 burst.
//   - SpawnBurst 의 spawn 순서가 결정론 (for i = 0..count).
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 8 Task 8.2.
//   - 메인 스펙 §3.4 — destructible terrain D2.5 명세.
// =============================================================================

#pragma once

#include "../core/Pool.h"
#include "../math/Vec2.h"
#include "../physics/PhysicsWorld.h"

// Chunk: 한 비산체의 메타 데이터.
//   - bodyId      : PhysicsWorld 의 인덱스. 죽으면 -1 또는 stale.
//   - lifetime    : 누적 살아있는 시간.
//   - maxLifetime : 자연 만료 시점 (1.5~2.5s, spawn 시 random).
struct Chunk {
    int   bodyId      = -1;
    float lifetime    = 0.0f;
    float maxLifetime = 2.0f;
};

class ChunkSpawner {
public:
    // 크기 / 물리 / 비산 속도 상수 — 본 클래스 내부에서 사용.
    static constexpr float kChunkHalf      = 3.0f;     // 6x6 px 정사각.
    static constexpr float kChunkMass      = 0.1f;
    // 비산 속도 (P8 시각 튜닝 단계 사용자 피드백): 200~400 → 300~600 으로 상향.
    //   - 더 활발한 잔해 비산 시각 — 폭발의 무게감 보강.
    //   - 일부 chunk 가 화면 가장자리까지 도달 가능 (offscreen despawn 정상).
    static constexpr float kBurstSpeedMin  = 300.0f;
    static constexpr float kBurstSpeedMax  = 600.0f;

    // collisionLayer / Mask (P8 Task 8.2 wiring).
    static constexpr unsigned int kLayer   = 1u << 2;  // Chunk.
    static constexpr unsigned int kMask    = (1u << 0) | (1u << 2) |
                                             (1u << 3) | (1u << 4);

    // 초기화: PhysicsWorld 와 풀 capacity.
    //   - capacity 는 한 라운드의 최대 동시 chunk 수 추정. 폭발 1 회 ~ 75 chunk
    //     (12 타일 × 6) → capacity 200~300 이 안전.
    void Init(PhysicsWorld& world, int capacity);

    // 한 spawn 지점에서 count 개의 chunk 를 사방으로 비산.
    //   - 풀 가득차면 일찍 종료 (할당 없음 — Pool 정책).
    //   - 각 chunk 의 방향은 random angle, 속도는 [kBurstSpeedMin, kBurstSpeedMax].
    void SpawnBurst(Vec2 origin, int count);

    // 매 fixed-step 호출. lifetime 진행 + 화면 밖 / 만료 시 자동 despawn.
    void Update(float dt);

    // P9 — 살아 있는 모든 chunk 즉시 despawn. 라운드 사이 맵 전환 시 이전
    // 라운드의 잔해가 다음 맵에 떠 있는 시각 회피용. PhysicsWorld 의 body 도
    // 함께 destroy.
    void Reset();

    // 외부 (main.cpp 의 렌더 / 디버그) 가 alive chunk 순회 + 활성 수 확인.
    Pool<Chunk>&       Storage()       { return pool_; }
    const Pool<Chunk>& Storage() const { return pool_; }

private:
    PhysicsWorld* world_ = nullptr;
    Pool<Chunk>   pool_;
};
