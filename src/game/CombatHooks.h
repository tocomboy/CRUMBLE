// =============================================================================
// src/game/CombatHooks.h — 투사체-플레이어 충돌 응답 훅 (Phase 5 추출).
// =============================================================================
//
// 목적:
//   - P3 MVP 단계에서는 투사체 데미지 처리 람다가 main.cpp 의 ContactCallback
//     안에 inline 으로 박혀 있었다. P5 에서 무기 종류가 3 가지 (Rifle / Shotgun
//     / Mortar) 로 늘면서 콜백 안의 분기가 길어지면 main.cpp 의 가독성이
//     급격히 떨어지므로, 본 헤더가 데미지 처리 책임을 한 곳으로 모은다.
//
// 본 함수가 담당하는 일:
//   - 한 contact 에서 양쪽 body 의 userData 를 검사해 Projectile 식별.
//   - 자기 owner 가 쏜 탄에는 맞지 않게 friendly-fire 필터.
//   - 명중 시 victim HP 차감 + Projectile.expired = true (deferred destruction —
//     실제 PhysicsWorld::DestroyBody 는 다음 ProjectilePool::Update 가 안전한
//     시점에 처리).
//   - Projectile 이 정적 지형 (벽 / 바닥) 에 맞으면 데미지 없이 expired 만
//     true (스폰된 탄이 영원히 떠다니지 않도록).
//
// 본 함수가 담당하지 않는 일 (호출자 = main.cpp 의 책임):
//   - Player.EvaluateContact 호출 (ground/wall 검출).
//   - Mortar 폭발 처리 (P8 에서 본 함수 안에 추가 예정).
//   - Knockback 임펄스 (P5 Task 5.6 에서 weapon kind 별 분기 추가 예정).
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 5 Task 5.1.
//   - 본 P3 시점의 main.cpp inline 람다 (lines 195~229) 가 본 함수의 정전.
//
// 결정론 / 스레드:
//   - PhysicsWorld::Step 안에서 contact callback 으로 호출된다. 즉 메인
//     스레드 단독 + fixed-timestep 결정론 보장.
//   - HP 차감 / expired 마킹만 수행 — body 파괴는 절대 호출하지 않는다 (콜백
//     도중 vector 무효화 위험).
// =============================================================================

#pragma once

#include "../math/Vec2.h"

class PhysicsWorld;
struct Contact;
class Player;
class ProjectilePool;
class TileGrid;
class ChunkSpawner;
class BreakableSlabField;

// HandleProjectileContact: 한 contact 의 투사체 데미지 처리.
//
//   - c     : PhysicsWorld 가 콜백에 넘겨주는 contact 구조 (a/b BodyId + normal).
//   - pp    : 투사체 풀 reference. 본 함수는 expired 만 마킹 — Despawn 즉시
//             호출하지 않으므로 사실상 사용하지 않지만, P5.6 이후 무기별
//             부가 효과 (예: Shotgun 명중 시 작은 임팩트 풀 spawn) 가 추가될
//             때를 대비해 시그니처에 포함.
//   - world : userData 캐스팅을 위해 BodyId → PhysicsBody* 조회에 사용.
//   - p1/p2 : 두 플레이어 reference. body 식별과 HP 차감을 위해 직접 들고온다.
//
//   콜백 도중 안전 규칙:
//     - victim HP 차감 / projectile.expired 마킹만 한다.
//     - DestroyBody / Player 상태 변경 / pool.Despawn 등 시뮬레이션 자료구조
//       를 흔드는 호출은 하지 않는다.
void HandleProjectileContact(const struct Contact& c,
                             ProjectilePool& pp,
                             PhysicsWorld& world,
                             Player& p1,
                             Player& p2);

// HandleMortarExplosion: 박격포 탄이 무언가에 닿을 때 호출.
//
//   - center  : 폭발 중심 — 탄의 contact 시점 위치. main.cpp 의 contact
//               callback 에서 본 함수 호출 직전에 추출.
//   - world   : radial impulse 부여를 위한 dynamic body 순회 + body 접근.
//   - tiles   : kRadius 안의 destructible tile 파괴 + onDestroyed 콜백으로
//               각 타일 중심에서 chunks.SpawnBurst 호출 (메인 스펙 §3.4 의
//               "6 청크/타일").
//   - chunks  : Chunk burst spawn.
//
// 동작 (메인 플랜 P8 Task 8.3 + 메인 스펙 §3.4):
//   1) tiles.DestroyInRadius(center, kRadius, onDestroyed) — 반경 안 파괴.
//      onDestroyed = [&](Vec2 c){ chunks.SpawnBurst(c, 6); }.
//   2) 모든 Dynamic body 순회 → kRadius 안이면 radial impulse (분리 방향).
//
// 결정론:
//   - 메인 스레드 단독 호출 (contact callback 안). PhysicsWorld 자료구조 변경
//     없음 (DestroyBody 는 TileGrid 가 명시적으로만 호출).
//   - SpawnBurst / DestroyInRadius 모두 결정론.
//
// 본 함수가 담당하지 않는 일:
//   - 폭발 사운드 / 비주얼 — P9 (audio) / P10 (폴리싱) 에서 보강.
//   - 카메라 흔들림 — P10.
void HandleMortarExplosion(Vec2 center,
                           PhysicsWorld& world,
                           TileGrid& tiles,
                           ChunkSpawner& chunks,
                           BreakableSlabField& slabs);
