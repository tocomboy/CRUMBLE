// =============================================================================
// src/game/Map.cpp — DevArena 하드코딩 + JSON 로더.
// =============================================================================
//
// 출처:
//   - 메인 플랜 P3 Task 3.4 Step 2 (DevArena 좌표) + P9 Task 9.1 (JSON 로더).
//   - 좌표계는 화면 좌상단 (0,0) → 우하단 (1280, 720). y 가 아래쪽 +.
//   - JSON schema 는 메인 스펙 §4.7 정의.
//
// JSON 의존:
//   - third_party/json.hpp (nlohmann/json single-include v3.11.3).
//   - 헤더 한 개 추가만으로 동작. 빌드 시 추가 라이브러리 / 링크 필요 없음.
// =============================================================================

#include "Map.h"

#include <fstream>
#include <stdexcept>

#include "../../third_party/json.hpp"

namespace {

// JSON 의 [x, y] 배열을 Vec2 로 변환.
//   - 배열 길이 < 2 거나 숫자가 아니면 nlohmann::json 이 자체적으로 type_error
//     를 던짐. 호출자 (LoadMapFromJson) 가 std::runtime_error 로 묶어 보고.
Vec2 ToVec2(const nlohmann::json& a) {
    return Vec2{a[0].get<float>(), a[1].get<float>()};
}

// 정적 블록의 JSON 표기: { "rect": [topLeftX, topLeftY, w, h] } → center/half.
//   - 메인 스펙 §4.7 의 좌상단/크기 표기를 PhysicsBody 의 center/half 로
//     변환. half = size / 2.
StaticTileDef ToStatic(const nlohmann::json& obj) {
    const auto& r = obj["rect"];
    Vec2 tl{r[0].get<float>(), r[1].get<float>()};
    Vec2 sz{r[2].get<float>(), r[3].get<float>()};
    return {tl + sz * 0.5f, sz * 0.5f};
}

}  // namespace

MapData LoadMapFromJson(const std::string& path) {
    // 1) 파일 열기. ifstream 의 fail bit 검사로 존재/권한 확인.
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("LoadMapFromJson: cannot open " + path);
    }

    // 2) 파싱. 실패 시 nlohmann::json::parse_error → 그대로 전파해도 되지만
    //    호출자 입장에서 메시지를 통일하기 위해 runtime_error 로 wrap.
    nlohmann::json j;
    try {
        f >> j;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("LoadMapFromJson: parse error in ")
                                 + path + ": " + e.what());
    }

    MapData m;

    // 3) 메타 (name / world / spawn / bgm).
    m.name = j.value("name", std::string("?"));
    if (j.contains("world")) {
        m.worldSize.x = j["world"].value("width",  1280.0f);
        m.worldSize.y = j["world"].value("height",  720.0f);
    }
    if (j.contains("spawn_p1")) m.spawnP1 = ToVec2(j["spawn_p1"]);
    if (j.contains("spawn_p2")) m.spawnP2 = ToVec2(j["spawn_p2"]);
    m.bgmKey = j.value("bgm", std::string("bgm_battle"));

    // 4) tiles.static — 정적 블록 (바닥 / 벽 / 천장).
    if (j.contains("tiles") && j["tiles"].contains("static")) {
        for (const auto& s : j["tiles"]["static"]) {
            m.staticBlocks.push_back(ToStatic(s));
        }
    }

    // 5) tiles.destructible — 박격포로 부서질 수 있는 영역. TileGrid 가
    //    LoadDestructibleArea(topLeft, size) 형식으로 받으므로 동일하게 변환.
    if (j.contains("tiles") && j["tiles"].contains("destructible")) {
        for (const auto& d : j["tiles"]["destructible"]) {
            const auto& r = d["rect"];
            DestructibleDef dd;
            dd.topLeft = Vec2{r[0].get<float>(), r[1].get<float>()};
            dd.size    = Vec2{r[2].get<float>(), r[3].get<float>()};
            m.destructibleAreas.push_back(dd);
        }
    }

    // 6) covers — 엄폐물. 정적 블록과 같은 PhysicsBody 표현이지만 kind hint.
    if (j.contains("covers")) {
        for (const auto& c : j["covers"]) {
            const auto& r = c["rect"];
            CoverDef cd;
            Vec2 tl{r[0].get<float>(), r[1].get<float>()};
            Vec2 sz{r[2].get<float>(), r[3].get<float>()};
            cd.center = tl + sz * 0.5f;
            cd.half   = sz * 0.5f;
            cd.kind   = c.value("kind", std::string("small"));
            m.covers.push_back(cd);
        }
    }

    // 7) moving_platforms — Kinematic 진동 발판. axis 는 첫 글자 (x|y).
    if (j.contains("moving_platforms")) {
        for (const auto& mp : j["moving_platforms"]) {
            const auto& r = mp["rect"];
            MovingPlatDef d;
            Vec2 tl{r[0].get<float>(), r[1].get<float>()};
            Vec2 sz{r[2].get<float>(), r[3].get<float>()};
            d.center    = tl + sz * 0.5f;
            d.half      = sz * 0.5f;
            d.axis      = mp.value("axis", std::string("x"))[0];
            d.amplitude = mp.value("amplitude", 80.0f);
            d.period    = mp.value("period",     3.0f);
            m.movingPlatforms.push_back(d);
        }
    }

    // 8) breakable_slabs — 박격포 폭발 시 통째로 무너지는 긴 벽/지붕 슬래브.
    //   JSON 스키마 (각 항목):
    //     { "rect": [topLeftX, topLeftY, w, h],
    //       "hp": int (optional, default 1),
    //       "color": [r, g, b] (optional, default [150, 120, 90]) }
    //   center = topLeft + size/2,  half = size/2 (ToStatic 와 동일한 변환).
    //   optional 키가 없으면 SlabDef 의 기본값을 그대로 사용.
    if (j.contains("breakable_slabs")) {
        for (const auto& s : j["breakable_slabs"]) {
            const auto& rect = s["rect"];
            Vec2 tl{rect[0].get<float>(), rect[1].get<float>()};
            Vec2 sz{rect[2].get<float>(), rect[3].get<float>()};
            SlabDef sd;
            sd.center = tl + sz * 0.5f;
            sd.half   = sz * 0.5f;
            sd.hp     = s.value("hp", 1);
            // color 배열 [r, g, b] — 없으면 SlabDef 의 기본값(150,120,90) 유지.
            if (s.contains("color") && s["color"].size() >= 3) {
                sd.r = static_cast<Uint8>(s["color"][0].get<int>());
                sd.g = static_cast<Uint8>(s["color"][1].get<int>());
                sd.b = static_cast<Uint8>(s["color"][2].get<int>());
            }
            m.slabs.push_back(sd);
        }
    }

    return m;
}

MapData MakeHardcodedDevMap() {
    // P3 의 DevArena 좌표를 본 함수에 보존 — JSON 로드 실패 / 헤드리스 빌드
    // 환경에서 회귀 검증을 가능하게 한다 (Plan Revisability 섹션의 회귀 가드).
    // P9 단계 추가: P6 의 좌/우 측벽 + P8 의 destructible 영역 + 가운데 위
    // moving platform 까지 포함해 실제 게임플레이와 동일한 환경을 제공.
    MapData m;
    m.name = "DevArena";
    m.spawnP1 = Vec2{300.0f, 400.0f};
    m.spawnP2 = Vec2{980.0f, 400.0f};

    // 바닥 (Floor) — 화면 거의 전체 가로 (half.x=640 → width=1280).
    m.staticBlocks.push_back({Vec2{640.0f, 700.0f}, Vec2{640.0f, 10.0f}});

    // 좌우 cover 블록 — 30x30 작은 박스 두 개.
    m.staticBlocks.push_back({Vec2{500.0f, 660.0f}, Vec2{30.0f, 30.0f}});
    m.staticBlocks.push_back({Vec2{780.0f, 660.0f}, Vec2{30.0f, 30.0f}});

    // 좌/우 측면 벽 (P6) — half=(20, 200) → 두께 40, 높이 400.
    m.staticBlocks.push_back({Vec2{30.0f,   490.0f}, Vec2{20.0f, 200.0f}});
    m.staticBlocks.push_back({Vec2{1250.0f, 490.0f}, Vec2{20.0f, 200.0f}});

    // 천장 — half.x=640, half.y=10. 점프가 화면 상단을 빠져나가지 않게.
    m.staticBlocks.push_back({Vec2{640.0f, 60.0f}, Vec2{640.0f, 10.0f}});

    // P8 — destructible 영역. 화면 가운데 (608, 600) ~ (672, 696) = 64x96 → 24 타일.
    m.destructibleAreas.push_back({Vec2{608.0f, 600.0f}, Vec2{64.0f, 96.0f}});

    // P6 — moving platform. (640, 480) X 축 ±200 / 4s. half=(60, 8).
    m.movingPlatforms.push_back({Vec2{640.0f, 480.0f}, Vec2{60.0f, 8.0f},
                                 'x', 200.0f, 4.0f});

    m.bgmKey = "bgm_battle";
    return m;
}
