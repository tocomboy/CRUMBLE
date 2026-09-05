// =============================================================================
// tests/test_tile_grid.cpp — TileGrid 단위 테스트 (3 케이스).
//
// 검증 대상:
//   1. LoadDestructibleArea 가 size / kTileSize 만큼의 정적 body 생성.
//   2. DestroyInRadius 가 반경 안의 alive 타일을 정확히 destroy.
//   3. RestoreAll 후 AliveCount 가 다시 cols * rows.
//
// 케이스 + 잡는 잠재 버그:
//   1. tilegrid_load_creates_static_bodies
//      — 64x32 영역, kTileSize=16 → 4×2 = 8 타일.
//      버그: kTileSize 가 다른 값으로 잘못 설정 / cols/rows 계산 오류.
//   2. tilegrid_destroy_in_radius
//      — 64x64 영역에서 (8,8) 반경 20 안의 타일 파괴 후 alive 수 감소.
//      버그: LengthSq 비교 오류 / alive_=false 마킹 누락.
//   3. tilegrid_restore_returns_all
//      — 파괴 후 RestoreAll → alive 모두 복원.
//      버그: bodyIds_ 갱신 누락 / alive_ flag 누락.
// =============================================================================

#include "assert_eq.h"
#include "test_runner.h"

#include "../src/game/TileGrid.h"
#include "../src/physics/PhysicsWorld.h"

REGISTER_TEST(tilegrid_load_creates_static_bodies) {
    PhysicsWorld w;
    TileGrid g(w);
    // 64x32 / 16 = 4x2 = 8 타일.
    g.LoadDestructibleArea(Vec2{0.0f, 0.0f}, Vec2{64.0f, 32.0f});
    CR_ASSERT(g.AliveCount() == 8);
    CR_ASSERT(g.Cols() == 4);
    CR_ASSERT(g.Rows() == 2);
}

REGISTER_TEST(tilegrid_destroy_in_radius) {
    PhysicsWorld w;
    TileGrid g(w);
    // 64x64 / 16 = 4x4 = 16 타일. (0,0) 좌상단 ~ (64,64) 우하단.
    g.LoadDestructibleArea(Vec2{0.0f, 0.0f}, Vec2{64.0f, 64.0f});
    // (8,8) 좌측 위쪽 첫 타일 중심 부근. radius 20 안에 적어도 1 타일 (8,8 자체)
    // — 인접한 (24,8)/(8,24) 타일의 중심까지의 거리 = 16, 16<20 이라 포함.
    //   대각선 (24,24) 까지의 거리 = sqrt(256+256) ≈ 22.6 > 20 → 미포함.
    //   따라서 4 타일 파괴 예상 (좌상단 (8,8), (24,8), (8,24), 그리고 LengthSq
    //   < 400 충족하는 모든).
    int destroyed = g.DestroyInRadius(Vec2{8.0f, 8.0f}, 20.0f);
    CR_ASSERT(destroyed > 0);
    CR_ASSERT(g.AliveCount() == 16 - destroyed);
}

REGISTER_TEST(tilegrid_restore_returns_all) {
    PhysicsWorld w;
    TileGrid g(w);
    // 32x32 / 16 = 2x2 = 4 타일.
    g.LoadDestructibleArea(Vec2{0.0f, 0.0f}, Vec2{32.0f, 32.0f});
    // 모든 타일 파괴 (반경 100 — 모든 타일 중심 < 100).
    g.DestroyInRadius(Vec2{0.0f, 0.0f}, 100.0f);
    CR_ASSERT(g.AliveCount() == 0);
    g.RestoreAll();
    CR_ASSERT(g.AliveCount() == 4);
}

// P9 결함 #15 회귀 가드 — multi-area additive load.
//
// 동기: P9 의 JSON 맵 (Bunker / Hideout) 이 한 라운드에 multiple destructible
// 영역을 보유. 이전 .assign 방식은 첫 N-1 영역의 PhysicsBody 가 vector 에서만
// 사라지고 PhysicsWorld 에 leak ("투명한 벽") 했다. 본 테스트는 두 영역을
// 연속 로드 후 alive 합계 + Clear 후 잔존 검사 + RestoreAll 후 합계 검증.
REGISTER_TEST(tilegrid_multi_area_additive) {
    PhysicsWorld w;
    TileGrid g(w);
    // Area 1: 64x32 → 4x2 = 8 타일.
    g.LoadDestructibleArea(Vec2{0.0f, 0.0f}, Vec2{64.0f, 32.0f});
    // Area 2: 32x32 → 2x2 = 4 타일. 다른 위치라 Area 1 과 안 겹침.
    g.LoadDestructibleArea(Vec2{200.0f, 200.0f}, Vec2{32.0f, 32.0f});

    // 두 영역의 합 = 12.
    CR_ASSERT(g.AliveCount() == 12);

    // PhysicsWorld 의 body 도 12 개 (Static body 만 12 개 등록되었으므로
    // BodyCount 가 12 이상 — 다른 body 가 없으므로 정확히 12).
    CR_ASSERT(w.BodyCount() == 12);

    // 한쪽 영역 (Area 2) 만 폭파 — Area 1 은 그대로 보존.
    const int destroyed = g.DestroyInRadius(Vec2{216.0f, 216.0f}, 32.0f);
    CR_ASSERT(destroyed == 4);
    CR_ASSERT(g.AliveCount() == 8);

    // RestoreAll 이 Area 1 + Area 2 모두 살림.
    g.RestoreAll();
    CR_ASSERT(g.AliveCount() == 12);

    // Clear 가 모든 영역의 body 일괄 파괴 — leak 없음.
    g.Clear();
    CR_ASSERT(g.AliveCount() == 0);
    CR_ASSERT(w.BodyCount() == 0);
}
