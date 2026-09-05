// =============================================================================
// src/game/CombatHooks.cpp — HandleProjectileContact 본문.
// =============================================================================
//
// 출처:
//   - 메인 플랜 P5 Task 5.1 의 코드 + 본 프로젝트 main.cpp P3 inline 람다와
//     동일 의미 보존 (회귀 0).
// =============================================================================

#include "CombatHooks.h"

#include <algorithm>
#include <cmath>

#include "BreakableSlab.h"
#include "ChunkSpawner.h"
#include "Player.h"
#include "Projectile.h"
#include "TileGrid.h"
#include "../physics/Collision.h"
#include "../physics/PhysicsBody.h"
#include "../physics/PhysicsWorld.h"

namespace {

// DamagePlayer: 한 발이 한 victim 에게 명중했을 때의 처리.
//   - pj.expired         : 이미 처리된 탄은 이중 데미지 방지를 위해 무시.
//   - ownerPlayerIndex   : 자기 자신이 쏜 탄에는 맞지 않음 (friendly fire 방지).
//   - HP 차감 후 expired = true 로 deferred destruction 트리거.
//
// 본 함수는 콜백 도중 한 번만 호출되므로 std::max(0, ...) 의 음수 클램프가
// 사실상 안전망 — HP 가 음수로 가지 않게 0 하한.
void DamagePlayer(Projectile& pj, Player& victim, int victimIdx) {
    if (pj.expired) return;
    if (pj.ownerPlayerIndex == victimIdx) return;
    victim.Stats().hp = std::max(0, victim.Stats().hp - pj.damage);
    pj.expired = true;
}

}  // namespace

void HandleProjectileContact(const Contact& c,
                             ProjectilePool& /*pp*/,
                             PhysicsWorld& world,
                             Player& p1,
                             Player& p2) {
    // BodyId → PhysicsBody* 조회. 둘 중 하나라도 null 이면 본 contact 가 이미
    // 정리된 body 를 참조 — 조용히 종료 (원본 main.cpp 의 if (!ba || !bb)
    // 가드와 동일).
    PhysicsBody* ba = world.GetBody(c.a);
    PhysicsBody* bb = world.GetBody(c.b);
    if (!ba || !bb) return;

    // userData 캐스팅 — Projectile 이 아닌 body 는 userData == nullptr 이므로
    // pa / pb 가 null 인 경우는 캐스팅 결과로도 nullptr 가 자연스럽게 나온다.
    // 단, P5.5 이후 Player 에 별도 userData 를 심으면 본 캐스팅이 깨지므로
    // 그 시점에서 식별 태그 (예: void* + 종류 enum) 도입 검토.
    Projectile* pa = static_cast<Projectile*>(ba->userData);
    Projectile* pb = static_cast<Projectile*>(bb->userData);

    // [Plan 결함 #10 — root-cause fix]
    //   Shotgun 의 6 펠릿이 같은 origin 에서 spawn 되면 첫 frame 에 펠릿
    //   끼리 narrow-phase contact 가 모든 쌍에서 검출. 본 함수의 일반 분기
    //   가 양쪽 다 Projectile 인 경우에도 expired 마킹을 하면 6 펠릿이
    //   즉시 모두 파괴되어 "발사 자체가 안 되는" 사용자 시각.
    //
    //   투사체끼리는 어차피 게임적으로 의미 있는 상호작용이 없다 (서로
    //   파괴? 튕김? 둘 다 아님 — 메인 스펙 §5 어디에도 명시 안 됨).
    //   따라서 양쪽 모두 Projectile 인 contact 는 단순히 무시 — 두 탄이
    //   서로를 통과한다.
    //
    //   대안 (rejected): collisionLayer/Mask 로 같은 owner 펠릿끼리만 분리.
    //     - 펠릿 spawn 시 owner 별 layer bit 설정 필요 → ProjectilePool 변경.
    //     - 다른 owner 의 탄 vs 탄 충돌도 무의미 → 어차피 모든 탄-탄 무시가
    //       정답. 본 분기 추가가 가장 간결.
    if (pa && pb) return;
    // 추가 효과: P3 시절에 한 손 (Rifle) 동시 발사로 같은 origin spawn 시
    //   잠재적 즉시 expired 위험이 본 변경으로 함께 해소. 시각 검증으로는
    //   보이지 않던 잠재 결함.

    // 한 람다로 양쪽 케이스 통일. otherId 는 "이 탄이 부딪힌 상대 body" 의
    // BodyId — 그 id 가 P1 / P2 / 그 외 (정적 지형 등) 인지로 분기.
    auto handle = [&](Projectile* pj, BodyId otherId) {
        if (!pj) return;
        if      (otherId == p1.GetBodyId()) DamagePlayer(*pj, p1, 0);
        else if (otherId == p2.GetBodyId()) DamagePlayer(*pj, p2, 1);
        else                                pj->expired = true;
        // 위 마지막 분기 (지형 명중) 는 P3 의 inline 람다에는 없었던 강화.
        // 원본은 정적 지형에 맞은 탄이 자연 lifetime (1.5s) 동안 떠다녔지만,
        // P5 의 Mortar 처럼 lifetime 5s 인 무거운 탄이 벽 안에 박혀 떠다니면
        // 시각적으로 어색하다. expired 마킹은 deferred destruction 패턴
        // (콜백 안에서 풀 자료구조 흔들지 않음) 을 그대로 따른다.
    };
    handle(pa, c.b);
    handle(pb, c.a);
}

// =============================================================================
// HandleMortarExplosion (P8 Task 8.3) — 박격포 폭발.
// =============================================================================

void HandleMortarExplosion(Vec2 center,
                           PhysicsWorld& world,
                           TileGrid& tiles,
                           ChunkSpawner& chunks,
                           BreakableSlabField& slabs) {
    // 폭발 반경 (P8 시각 튜닝 단계 사용자 피드백):
    //   80 → 100 → 60 px. 100 은 destructible 영역 (64 폭) 보다 큰 반경이라
    //   한 발에 거의 전체 파괴 → 둘째 라운드까지 시각 단조. 60 으로 축소해
    //   여러 발이 필요한 점진 파괴 + 폭발의 한정된 영향권.
    constexpr float kRadius = 60.0f;

    // 1) 반경 안 destructible 타일 파괴 + 각 파괴 위치에 6 chunks burst.
    //    DestroyInRadius 가 onDestroyed 콜백을 alive_ 인덱스 순으로 호출 →
    //    chunk burst 순서가 결정론.
    tiles.DestroyInRadius(center, kRadius, [&](Vec2 tileCenter) {
        chunks.SpawnBurst(tileCenter, 6);
    });

    // 1.5) 폭발 반경에 닿은 breakable slab 처리 — hp 소진 시 슬래브 전체가
    //      한꺼번에 붕괴하고, 슬래브 면적 전체에 debris burst (TileGrid 와
    //      동일한 onDebris 콜백 패턴). 긴 지붕/벽이 "통째로" 무너진다.
    slabs.OnExplosion(center, kRadius, world, [&](Vec2 p) {
        chunks.SpawnBurst(p, 6);
    });

    // 2) 모든 Dynamic body 에 radial impulse 부여 (D2.5 — 폭풍 충격).
    //    - kBlast    : 거리 0 일 때의 임펄스 강도 계수 (질량 곱 후).
    //    - kFalloff  : 거리에 따른 감쇠 분모 — kFalloff 거리에서 강도 1 배,
    //                  그 이상에서 점차 약해짐.
    //    - dist=0 일 때 dir = 위쪽 (-y) 으로 fallback (NaN 회피).
    // Blast 강도 (P8 시각 튜닝 단계 사용자 피드백):
    //   원안 5 는 ApplyImpulse 의 단위 (vel += impulse × invMass) 로 환산 시
    //   player(mass=1) 에 vel.delta=5 px/s 만 부여 — 시각적으로 느껴지지 않음.
    //   100 으로 상향 → max strength=100 일 때 player vel +=100 px/s, 평균 거리
    //   에서 30~50 px/s. "폭발에 밀려난다" 가 시각적으로 명확.
    //   Chunk (mass=0.1) 은 같은 strength 에 대해 더 큰 vel 변화 — 잔해 비산
    //   효과 자연스러움 (impulse * mass 곱이라 가벼울수록 vel 큰 변화).
    constexpr float kBlast   = 100.0f;
    constexpr float kFalloff = 40.0f;
    const int cap = world.BodyCapacity();
    for (int i = 0; i < cap; ++i) {
        PhysicsBody* b = world.GetBody(i);
        if (!b) continue;
        if (b->type != BodyType::Dynamic) continue;

        const Vec2  d    = b->pos - center;
        const float dist = d.Length();
        if (dist > kRadius) continue;

        // 폭발 중심에서 본 body 까지의 단위 방향.
        const Vec2  dir = (dist > 1e-3f)
                          ? Vec2{d.x / dist, d.y / dist}
                          : Vec2{0.0f, -1.0f};
        // 거리 falloff: dist=0 ~ kFalloff 구간은 동일 강도, 그 이상은 1/x 감쇠.
        const float strength = kBlast / std::max(dist / kFalloff, 1.0f);
        // 임펄스 = 단위 방향 × strength × mass — 같은 strength 라도 가벼운
        // body 가 더 빠르게 가속 (물리적 정합).
        b->ApplyImpulse(dir * (strength * b->mass));
    }
}
