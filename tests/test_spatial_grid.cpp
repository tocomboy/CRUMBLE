// =============================================================================
// tests/test_spatial_grid.cpp — SpatialGrid 단위 테스트 (4 케이스).
//
// 검증 대상:
//   1. 단일 body 등록 시 pair 0개.
//   2. 같은 cell 안 두 body 는 정확히 1 pair.
//   3. 두 body 가 cell 경계에 걸쳐 여러 cell 에 등장해도 dedup 후 1 pair.
//   4. 멀리 떨어진 두 body 는 pair 0개.
//
// 케이스 + 잡는 잠재 버그:
//   1. grid_inserts_body_in_correct_cell
//      — Insert 후 QueryPairs 가 단일 body 라 비어야 함.
//      버그: cell 인덱스 계산 오류로 같은 body 가 두 번 등장 → self-pair.
//   2. grid_finds_pairs_in_same_cell
//      — 같은 cell 안 두 body 가 pair 1개 검출.
//      버그: cells_ 의 push_back 누락, QueryPairs 의 i<j 미준수.
//   3. grid_no_duplicate_pairs_across_cells
//      — body 두 개가 각 4 cell 에 걸쳐 raw pair 가 4 개 발생 가능,
//        sort+unique 로 1 개로 dedup 되어야 함.
//      버그: dedup 로직 누락 시 pair 4개로 늘어남.
//   4. grid_far_bodies_no_pair
//      — 떨어진 두 body 는 같은 cell 에 들어가지 않으므로 pair 없음.
//      버그: cell 인덱스 wrap-around / clamp 오류.
//
// 결정론 검증은 별도 — P4 의 byte-equal replay test 가 SpatialGrid 통합
// 후에도 동일 결과 유지하면 자동 검증.
// =============================================================================

#include "assert_eq.h"
#include "test_runner.h"

#include "../src/physics/SpatialGrid.h"

#include <algorithm>
#include <utility>
#include <vector>

// ---- 1. 단일 body 등록 → pair 0 ----
REGISTER_TEST(grid_inserts_body_in_correct_cell) {
    SpatialGrid g(32.0f, Vec2{0.0f, 0.0f}, Vec2{1280.0f, 720.0f});
    g.Clear();
    g.Insert(0, AABB{Vec2{50.0f, 50.0f}, Vec2{8.0f, 8.0f}});

    std::vector<std::pair<int,int>> pairs;
    g.QueryPairs(pairs);

    CR_ASSERT(pairs.empty());
}

// ---- 2. 같은 cell 안 두 body → 정확히 1 pair ----
REGISTER_TEST(grid_finds_pairs_in_same_cell) {
    SpatialGrid g(64.0f, Vec2{0.0f, 0.0f}, Vec2{640.0f, 480.0f});
    g.Clear();
    // 둘 다 cell (0, 0) 안 — cell 좌표 [0, 64) × [0, 64).
    g.Insert(0, AABB{Vec2{50.0f, 50.0f}, Vec2{8.0f, 8.0f}});
    g.Insert(1, AABB{Vec2{60.0f, 50.0f}, Vec2{8.0f, 8.0f}});

    std::vector<std::pair<int,int>> pairs;
    g.QueryPairs(pairs);

    CR_ASSERT(pairs.size() == 1);
    CR_ASSERT(pairs[0].first == 0 && pairs[0].second == 1);
}

// ---- 3. cell 경계에 걸친 두 body — dedup 후 1 pair ----
REGISTER_TEST(grid_no_duplicate_pairs_across_cells) {
    SpatialGrid g(20.0f, Vec2{0.0f, 0.0f}, Vec2{400.0f, 400.0f});
    g.Clear();
    // half=15 인 두 body 가 (20, 20), (40, 20) 에 위치 → 둘 다 4 개 cell 에 걸침
    // (대략 cell (0,0), (1,0), (0,1), (1,1) 또는 (1,0), (2,0), (1,1), (2,1) 등).
    // raw pair 가 두 cell 이상에서 검출 → sort+unique 로 dedup 되어야 1.
    g.Insert(0, AABB{Vec2{20.0f, 20.0f}, Vec2{15.0f, 15.0f}});
    g.Insert(1, AABB{Vec2{40.0f, 20.0f}, Vec2{15.0f, 15.0f}});

    std::vector<std::pair<int,int>> pairs;
    g.QueryPairs(pairs);

    CR_ASSERT(pairs.size() == 1);
}

// ---- 4. 멀리 떨어진 두 body → pair 없음 ----
REGISTER_TEST(grid_far_bodies_no_pair) {
    SpatialGrid g(32.0f, Vec2{0.0f, 0.0f}, Vec2{1280.0f, 720.0f});
    g.Clear();
    g.Insert(0, AABB{Vec2{50.0f, 50.0f}, Vec2{8.0f, 8.0f}});
    g.Insert(1, AABB{Vec2{500.0f, 500.0f}, Vec2{8.0f, 8.0f}});

    std::vector<std::pair<int,int>> pairs;
    g.QueryPairs(pairs);

    CR_ASSERT(pairs.empty());
}
