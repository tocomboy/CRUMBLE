// =============================================================================
// src/game/Map.h — 한 라운드의 지형 + 스폰 데이터 + 라운드별 셔플.
// =============================================================================
//
// 본 헤더의 정책 (P9 Task 9.1 갱신):
//   - P3 의 하드코딩 한 장 (DevArena) 정책을 폐기하고 JSON 3 종 맵 (Broken
//     Bridge / Bunker / Hideout) 을 로드 (메인 스펙 §4.7 / 본 플랜 P9 Task 9.1).
//   - MapData 가 정적 블록 외에도 destructible area / cover / moving platform /
//     bgm key 등 한 라운드의 모든 구성을 담는다 → main.cpp 의 LoadMap 이
//     본 데이터를 그대로 PhysicsWorld + TileGrid + MovingPlatform 으로 펼친다.
//   - MakeHardcodedDevMap 은 회귀 가드용 fallback 로 남긴다 (헤드리스 빌드 /
//     초기화 실패 시 사용).
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 3 Task 3.4
//     (DevArena 시초) + Phase 9 Task 9.1 (JSON 로더).
//   - 메인 스펙 §4.7 Map schema.
//   - [6. Resources.pdf] 의 ResourceManager 패턴 — 본 헤더 자체는 자원
//     로딩 책임 없음. JSON 파싱은 third_party/json.hpp single-include 사용.
//
// 결정론:
//   - LoadMapFromJson 은 디스크 읽기 + nlohmann::json 파싱 → 입력 JSON 이
//     같으면 같은 결과. shuffle 은 main.cpp 의 MapPool 단계에서 std::mt19937
//     으로 별도 처리 (본 헤더에는 셔플 로직 없음).
// =============================================================================

#pragma once

#include <string>
#include <vector>

#include "../math/Vec2.h"
#include "BreakableSlab.h"

// -----------------------------------------------------------------------------
// 정적 블록: 라운드 중 변하지 않는 AABB 한 덩어리. 바닥 / 벽 / 천장 등.
//   - PhysicsBody 의 pos/half 와 동일한 의미 — main.cpp 의 LoadMap 이 그대로
//     복사해 BodyType::Static 으로 등록.
// -----------------------------------------------------------------------------
struct StaticTileDef {
    Vec2 center;
    Vec2 half;
};

// -----------------------------------------------------------------------------
// Cover: 정적 블록의 일종이지만 "엄폐물" 의미. 본 P9 단계에서는 PhysicsBody 측
// 로는 정적 블록과 같지만 (kind 만 추가) 향후 D3 (cover 파괴) 가 도입되면
// 본 구조가 별도 처리될 수 있으므로 분리 정의.
//   - 현재 (P9): main.cpp 의 LoadMap 이 staticBlocks 와 동일하게 등록.
//   - kind: "small" | "large" — 시각적 크기 hint (현재는 단순 정보).
// -----------------------------------------------------------------------------
struct CoverDef {
    Vec2        center;
    Vec2        half;
    std::string kind;
};

// -----------------------------------------------------------------------------
// Destructible: 박격포로 부서질 수 있는 영역.
//   - topLeft : 영역의 (좌상단 x, y). TileGrid.LoadDestructibleArea 가 그대로
//               받는 형식.
//   - size    : (totalW, totalH). kTileSize=16 의 정수 배수 권장.
// -----------------------------------------------------------------------------
struct DestructibleDef {
    Vec2 topLeft;
    Vec2 size;
};

// -----------------------------------------------------------------------------
// MovingPlatform: Kinematic 정현파 발판.
//   - axis      : 'x' 또는 'y' (단일 축).
//   - amplitude : 진폭 (px).
//   - period    : 주기 (s).
// -----------------------------------------------------------------------------
struct MovingPlatDef {
    Vec2  center;
    Vec2  half;
    char  axis      = 'x';
    float amplitude = 80.0f;
    float period    = 3.0f;
};

// -----------------------------------------------------------------------------
// MapData: 한 라운드의 모든 정적 / kinematic 정보.
//   - name              : HUD / 디버그 표시용. JSON 의 "name" 필드.
//   - worldSize         : 화면 크기. P9 단계의 1280x720 가정. JSON 의 "world".
//   - spawnP1/P2        : 두 플레이어 라운드 시작 좌표.
//   - staticBlocks      : 정적 AABB 묶음 (바닥 / 벽).
//   - destructibleAreas : 파괴 가능한 직사각형 영역 (TileGrid 가 펼침).
//   - covers            : 엄폐물 (정적 AABB + kind hint).
//   - movingPlatforms   : Kinematic 진동 발판.
//   - slabs             : 박격포 폭발 시 통째로 무너지는 긴 벽/지붕 슬래브.
//                         JSON 의 "breakable_slabs" 배열 (optional, 없으면 비어있음).
//   - bgmKey            : 본 라운드에서 재생할 BGM 의 ResourceManager 키.
// -----------------------------------------------------------------------------
struct MapData {
    std::string                  name;
    Vec2                         worldSize{1280.0f, 720.0f};
    Vec2                         spawnP1{300.0f, 400.0f};
    Vec2                         spawnP2{980.0f, 400.0f};
    std::vector<StaticTileDef>   staticBlocks;
    std::vector<DestructibleDef> destructibleAreas;
    std::vector<CoverDef>        covers;
    std::vector<MovingPlatDef>   movingPlatforms;
    std::vector<SlabDef>         slabs;
    std::string                  bgmKey = "bgm_battle";
};

// LoadMapFromJson:
//   - path 의 JSON 파일을 읽어 MapData 로 변환.
//   - 누락된 optional 필드 (covers / moving_platforms / world / spawn_p?) 는
//     기본값으로 채움 — 본 헤더의 MapData 디폴트 값을 사용.
//   - 파일을 못 열거나 JSON 파싱이 실패하면 std::runtime_error 던짐 — 호출자
//     (main.cpp) 가 try/catch 로 처리.
MapData LoadMapFromJson(const std::string& path);

// MakeHardcodedDevMap:
//   - DevArena 한 장. 바닥 + 좌우 cover + 천장 + 좌/우 측벽 + 가운데 destructible
//     + 가운데 위 moving platform.
//   - JSON 로드 실패 시 회귀 가드용 fallback. P3 시점부터 회귀 테스트가
//     의존하는 핵심 시각 검증 환경.
MapData MakeHardcodedDevMap();
