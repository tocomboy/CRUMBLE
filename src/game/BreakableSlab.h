// =============================================================================
// src/game/BreakableSlab.h — 한 방에 전체가 무너지는 긴 벽/지붕 슬래브.
//
// 목적:
//   - TileGrid 가 타일 단위로 부서지는 것과 달리, BreakableSlab 은 하나의
//     Static body 전체가 박격포 폭발 시 한꺼번에 사라진다.
//   - 긴 플랫폼 / 두꺼운 지붕처럼 "한 번 맞으면 통째로 무너져야" 하는
//     지형에 사용. 폭발 반경이 AABB 에 닿으면 hp -= 1, hp == 0 이면
//     body + 시각 모두 즉시 제거.
//
// 동작 모델:
//   - Load(world, defs):
//       SlabDef 마다 Static AABB body 를 PhysicsWorld 에 등록.
//       hp, color, alive 를 parallel vector 로 보관 (TileGrid 패턴 동일).
//   - OnExplosion(center, radius, world, onDebris):
//       살아 있는 슬래브 중 AABB-원 overlap 인 것을 찾아 hp -= 1.
//       hp <= 0 → body destroy + alive=false + onDebris(p) 를 슬래브 전체에
//       격자 간격 (kDebrisStep) 으로 호출 → ChunkSpawner 가 debris 생성.
//   - RestoreAll(world): 라운드 시작 시 모든 슬래브 복원 + hp 원복.
//   - Clear(world): 맵 전환 시 모든 body 제거 + vector 초기화.
//   - Render(r): alive 슬래브를 color 로 채운 사각형 한 개로 그림.
//
// 결정론:
//   - 모든 메서드는 메인 스레드 단독.
//   - OnExplosion 은 인덱스 순 (결정론).
//   - AABB-원 overlap: closest-point-on-AABB → 거리² < radius².
//   - world.MarkStaticDirty() — body 변화 직후 호출 (TileGrid 패턴).
//
// 결합:
//   - onDebris 콜백으로 ChunkSpawner 의존 없음 (TileGrid 와 동일한 디커플).
//   - main.cpp 의 HandleMortarExplosion 이 동일 lambda 를 TileGrid 와
//     BreakableSlabField 모두에 연결 (메인 플랜 §3.4 콜백 패턴).
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 8 Task 8.1
//     (TileGrid 패턴).
//   - 메인 스펙 §3.4 D2.5 — 폭발에 의한 지형 파괴 명세.
// =============================================================================

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include <SDL.h>

#include "../math/Vec2.h"
#include "../physics/PhysicsWorld.h"

// -----------------------------------------------------------------------------
// SlabDef — JSON 파싱 결과 또는 하드코딩 맵 데이터.
//   - center / half : PhysicsBody 와 동일한 의미 (world 좌표 기준).
//   - hp            : 폭발 몇 번에 무너지는가. 1 = 한 방.
//   - r, g, b       : 렌더 색상. 회색 정적 벽과 구분되는 흙/나무색 기본값.
// -----------------------------------------------------------------------------
struct SlabDef {
    Vec2  center;
    Vec2  half;
    int   hp   = 1;
    Uint8 r    = 150;
    Uint8 g    = 120;
    Uint8 b    = 90;
};

// -----------------------------------------------------------------------------
// BreakableSlabField — 맵 한 장의 슬래브 집합 관리자.
//   - parallel vector (defs_ / bodyIds_ / alive_ / currentHp_) 는 TileGrid
//     의 alive_ / bodyIds_ / centers_ 와 같은 설계.
//   - 인덱스 i 가 하나의 슬래브를 가리킴.
// -----------------------------------------------------------------------------
class BreakableSlabField {
public:
    // 슬래브 debris 격자 간격 (px). 슬래브 전체에 걸쳐 debris 포인트를 분산.
    //   24~32 px 범위가 청크 스폰 밀도와 화면 크기의 균형.
    static constexpr float kDebrisStep = 28.0f;

    // Load — SlabDef 목록으로부터 Static body 생성 + 내부 상태 초기화.
    //   - 호출 전 Clear 불필요 (Load 내부에서 덮어씀).
    //   - 빈 defs 로 호출 가능 (슬래브 없는 맵에서도 안전).
    void Load(PhysicsWorld& world, const std::vector<SlabDef>& defs);

    // Clear — 살아 있는 모든 body 제거 + vector 비움 (맵 전환 시 호출).
    //   - TileGrid::Clear 와 동일한 책임.
    void Clear(PhysicsWorld& world);

    // RestoreAll — 모든 슬래브 body 재생성 + hp 원복 (라운드 시작 시 호출).
    //   - 지형 손상이 라운드 사이 지속되지 않는다 (메인 스펙 §3.4 invariant).
    //   - TileGrid::RestoreAll 과 동일한 책임.
    void RestoreAll(PhysicsWorld& world);

    // OnExplosion — 폭발 반경과 겹치는 alive 슬래브를 처리.
    //
    //   알고리즘:
    //     for each alive slab i:
    //       closest = clamp(center, slabMin, slabMax)
    //       if (center - closest).LengthSq() < radius²:
    //           currentHp_[i] -= 1
    //           if currentHp_[i] <= 0:
    //               alive_[i] = false
    //               DestroyBody(bodyIds_[i])
    //               onDebris(p) 를 슬래브 AABB 격자(kDebrisStep) 로 호출
    //
    //   반환: 이번 호출에서 파괴(hp <= 0) 된 슬래브 수.
    //   결정론: 인덱스 순 순회.
    int OnExplosion(Vec2 center, float radius, PhysicsWorld& world,
                    const std::function<void(Vec2)>& onDebris);

    // Render — alive 슬래브를 SlabDef 의 color 로 채운 사각형으로 그림.
    //   - 물리 body 의 AABB 와 정확히 일치 (invisible wall / phantom 없음):
    //       x = int(center.x - half.x),  y = int(center.y - half.y)
    //       w = int(half.x * 2),         h = int(half.y * 2)
    //   - 죽은 슬래브는 body 도 없고 렌더도 없음 — 동시 제거 보장.
    void Render(SDL_Renderer* r) const;

    // 디버그 / 테스트용 조회.
    int  AliveCount() const;
    int  TotalCount() const { return static_cast<int>(defs_.size()); }

private:
    // parallel vectors — 인덱스 i 가 같은 슬래브를 가리킴.
    std::vector<SlabDef> defs_;       // 원본 정의 (RestoreAll 때 hp 원복에 사용).
    std::vector<int>     bodyIds_;    // alive=true 면 valid BodyId, false 면 -1.
    std::vector<bool>    alive_;      // 슬래브 활성 여부.
    std::vector<int>     currentHp_;  // 현재 남은 hp (폭발마다 감소).
};
