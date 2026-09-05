// =============================================================================
// src/game/TileGrid.h — 파괴 가능한 타일 격자.
//
// 목적:
//   - 박격포 폭발이 닿는 정적 지형을 작은 타일 단위로 분해하여 폭발 반경 안의
//     타일만 정확히 destroy 할 수 있게 한다.
//   - 라운드 시작 시 RestoreAll 로 모든 타일 복원 (메인 스펙 §3.4 — 라운드
//     사이 지형 손상 비-지속).
//
// 동작 모델:
//   - LoadDestructibleArea(topLeft, size):
//       size.x / kTileSize × size.y / kTileSize 만큼의 정적 AABB body 를
//       PhysicsWorld 에 등록. 각 타일의 alive_ flag 와 bodyId 를 보관.
//   - DestroyInRadius(center, radius, onDestroyed):
//       유클리드 거리 < radius 인 alive 타일을 모두 destroy + onDestroyed
//       콜백 호출 (ChunkSpawner.SpawnBurst 트리거에 사용).
//       파괴 후 world.MarkStaticDirty() 호출 — P7 의 staticGrid_ 가 다음 step
//       에 rebuild 되도록.
//   - RestoreAll(): 모든 타일을 다시 PhysicsWorld 에 등록.
//
// 결정론:
//   - 모든 메서드는 메인 스레드 단독.
//   - 산식 (LengthSq < radius²) 은 부동소수 결정론.
//   - 콜백 호출 순서가 결정론: alive_ vector 인덱스 (col + row * cols_) 순.
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 8 Task 8.1.
//   - 메인 스펙 §3.4 D2.5 destructible terrain 명세.
// =============================================================================

#pragma once

#include <functional>
#include <vector>

#include "../math/Vec2.h"
#include "../physics/PhysicsWorld.h"

class TileGrid {
public:
    // 타일 한 변 길이 (px). 16 px → 폭발 반경 80 px 안에 약 5x5 = 25 타일.
    //   - 너무 작으면 body 수 폭증 (광역 단계 부담), 너무 크면 폭발 모양이
    //     "픽셀 풍" 거칠어짐. 16 이 본 게임 해상도 (1280×720) 의 균형.
    static constexpr float kTileSize = 16.0f;

    explicit TileGrid(PhysicsWorld& world);

    // 직사각형 영역을 파괴 가능한 타일들로 채움.
    //   - topLeft : 영역의 (좌상단 x, 좌상단 y) — Map.cpp 의 좌표계 기준.
    //   - size    : (totalW, totalH) — kTileSize 의 정수 배수가 권장.
    //               정수 배수가 아니면 마지막 줄/열이 잘림 (cols/rows 가
    //               static_cast<int> 의 floor).
    void LoadDestructibleArea(Vec2 topLeft, Vec2 size);

    // center 로부터 radius 안의 alive 타일을 모두 destroy.
    //   - 반환: 파괴된 타일 수.
    //   - onDestroyed 콜백: 파괴된 각 타일의 center 좌표를 받음. ChunkSpawner
    //     가 본 좌표에서 burst 호출 (메인 플랜 §3.4 — 6 청크/타일).
    //   - 한 개라도 파괴되면 world.MarkStaticDirty() 호출 → P7 grid rebuild.
    int DestroyInRadius(Vec2 center, float radius,
                        std::function<void(Vec2)> onDestroyed = nullptr);

    // 모든 타일 복원. 라운드 시작 시 호출.
    void RestoreAll();

    // P9 — 모든 타일 + body 제거. 라운드 사이 맵 셔플 시 LoadDestructibleArea
    // 호출 전에 본 메서드로 이전 맵의 타일을 깨끗이 정리한다.
    //   - alive 타일은 PhysicsWorld 에서 DestroyBody 호출.
    //   - 내부 vector 모두 비움 (cols_/rows_ 도 0).
    //   - world.MarkStaticDirty 호출.
    void Clear();

    // 디버그 / 시각화 / 테스트.
    int  AliveCount() const;
    int  Cols() const { return cols_; }
    int  Rows() const { return rows_; }
    bool IsAlive(int col, int row) const;
    Vec2 TileCenter(int col, int row) const;

    // 외부 (main.cpp 의 렌더 / 디버그) 가 alive 타일을 효율적으로 순회하기 위한
    // const view. 인덱스 = col + row * cols_.
    const std::vector<bool>& AliveFlags() const { return alive_; }
    const std::vector<Vec2>& Centers()    const { return centers_; }

private:
    PhysicsWorld& world_;
    Vec2          origin_{0.0f, 0.0f};
    int           cols_ = 0;
    int           rows_ = 0;
    // 모두 cols_ * rows_ 길이.
    std::vector<bool> alive_;       // 각 타일의 활성 여부.
    std::vector<int>  bodyIds_;     // alive_=true 면 BodyId, false 면 -1.
    std::vector<Vec2> centers_;     // 미리 계산된 타일 중심.
};
