// =============================================================================
// src/physics/SpatialGrid.h — 균일 grid 해시 broad-phase.
//
// 목적:
//   - 매 step 모든 body 쌍을 O(N²) 으로 검사하던 DetectContactsNaive 를 대체.
//   - body 의 AABB bound 를 grid cell 에 등록 → 같은 cell 안에 든 body 끼리만
//     pair 후보로 잡아 narrow-phase 호출 횟수를 줄인다.
//   - 200+ Dynamic body 환경에서 60 FPS 유지 (P7 acceptance gate).
//
// 알고리즘:
//   - 1) Clear()                   : 모든 cell 비움.
//   - 2) Insert(id, AABB)          : body 의 bound 가 걸치는 모든 cell 에 id push.
//   - 3) QueryPairs(out)           : 각 cell 안의 모든 (i, j) 쌍 (i < j) 을 모은 후
//                                     sort + unique 로 cell 경계에 걸친 body 의
//                                     중복 페어 제거.
//   - cell 크기는 일반 권장 — body 평균 크기의 2~4 배. 본 단계 dynamic 32 px.
//
// 결정론 / 스레드:
//   - 모든 메서드는 메인 스레드 단독 호출.
//   - QueryPairs 는 sort 후 unique → 같은 입력에 같은 출력 보장.
//     pair 의 정의 (a, b) 에서 a = min(id), b = max(id) 로 표준화하므로 두 번
//     같은 두 body 를 등록해도 (a, b) 단일 표현.
//   - PhysicsWorld 의 Step 이 본 broad-phase 의 결과 contacts 를 contactsThisStep_
//     에 푸시하는 순서가 결정론 영향. 본 모듈은 sort 된 (i, j) pair 를 반환하므로
//     PhysicsWorld 가 그 순서대로 추가하면 naive 와 동일한 contact 순서가 보장.
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 7 Task 7.1.
//   - [10. Physics Engine 1.pdf p.6-8] — Broad Phase 개요 + Spatial Hash Grid
//     설계 패턴 (cellSize 선정 기준, Insert/QueryPairs 구조). 본 클래스의
//     알고리즘과 직접 대응한다.
// =============================================================================

#pragma once

#include <utility>
#include <vector>

#include "../math/Vec2.h"

// AABB: SpatialGrid 가 받는 단순 좌표 묶음.
//   - PhysicsBody 와 같은 (center, half) 의미. 의존성 차단을 위해 별도 구조체.
//   - Circle body 는 호출자가 half = (r, r) 로 정렬해 넘긴다 (PhysicsWorld 에서 수행).
struct AABB {
    Vec2 center;
    Vec2 half;
};

class SpatialGrid {
public:
    // 생성자:
    //   - cellSize : 한 cell 의 한 변 길이 (px).
    //   - worldMin / worldMax : grid 가 커버하는 월드 영역. body 가 본 영역 밖
    //                           으로 나가도 안전하게 clamp 된 cell 인덱스로 처리.
    SpatialGrid(float cellSize, Vec2 worldMin, Vec2 worldMax);

    // Clear: 모든 cell 비움. 매 step 시작 시 호출.
    void Clear();

    // Insert: body 의 bound 가 걸치는 모든 cell 에 bodyId 를 push.
    //   - bound 가 grid 영역 밖이면 clamp 된 가장 가까운 cell 에 등록 (안전망).
    //   - 한 body 가 여러 cell 에 동시 등록 (큰 body 는 자연스럽게 여러 cell).
    void Insert(int bodyId, const AABB& bounds);

    // QueryPairs: 같은 cell 에 든 body 쌍을 (a, b) (a < b) 형태로 outPairs 에
    //   채움. cell 경계에 걸친 body 의 중복 페어는 sort + unique 로 dedup.
    //   - outPairs 는 호출자가 미리 vector 를 들고 와 넘김 (재사용 → 할당 회피).
    //   - 결과는 정렬된 상태 (결정론).
    void QueryPairs(std::vector<std::pair<int,int>>& outPairs) const;

    // QueryAABB: 주어진 bound 와 겹치는 cell 들에 든 body id 들을 outIds 로 채움.
    //   - 중복 dedup 후 정렬해 반환 (결정론).
    //   - PhysicsWorld 의 dynamic vs static 분리 검사에 사용.
    void QueryAABB(const AABB& bounds, std::vector<int>& outIds) const;

    // 디버그 / 시각화 / 테스트 용 접근자.
    int   CellCount() const { return cols_ * rows_; }
    int   Cols()      const { return cols_; }
    int   Rows()      const { return rows_; }
    float CellSize()  const { return cellSize_; }
    Vec2  WorldMin()  const { return worldMin_; }
    Vec2  WorldMax()  const { return worldMax_; }

private:
    float cellSize_;
    Vec2  worldMin_;
    Vec2  worldMax_;
    int   cols_;
    int   rows_;
    // cells_[cy * cols_ + cx] → bodyId 목록.
    std::vector<std::vector<int>> cells_;

    int CellIdx(int cx, int cy) const { return cx + cy * cols_; }
};
