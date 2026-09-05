// =============================================================================
// src/game/MapEditor.cpp — 인게임 마우스 맵 에디터 구현.
//
// 개요:
//   RunMapEditor() 를 호출하면 독립적인 SDL 이벤트+렌더 루프가 시작된다.
//   사용자가 ESC 를 누르거나 창을 닫으면 반환한다.
//
// 오브젝트 타입 (숫자 키 선택):
//   1 = Static wall       — 회색   (90,  90,  90)
//   2 = Destructible tile — 밀색   (175, 140, 105)
//   3 = Breakable slab    — 흙색   (150, 120,  90)
//   4 = Cover (small)     — 회청색 (100, 130, 160)
//   5 = Cover (large)     — 회청색 (80,  110, 145)
//   6 = Spawn P1          — 빨간색 마커
//   7 = Spawn P2          — 파란색 마커
//   8 = Moving platform   — 청록색 (60,  200, 200)
//
// 마우스 조작:
//   LMB 드래그  — 사각형 배치 (16px 그리드 스냅, 좌상단→우하단 순서 보정)
//   LMB 클릭    — Spawn 타입(6/7) 은 단일 클릭으로 배치
//   RMB 클릭    — 커서 아래 최상단 오브젝트 삭제
//   드래그 중   — 반투명 미리보기 사각형 표시
//
// 단축키:
//   1~8  — 오브젝트 타입 선택
//   F1   — Broken Bridge 로드 (assets/maps/broken_bridge.json)
//   F2   — Bunker 로드 (assets/maps/bunker.json)
//   F3   — Hideout 로드 (assets/maps/hideout.json)
//   N    — 새 맵 (기본 경계 벽 + 기본 스폰 위치)
//   S    — 저장 (로드한 경로 → 없으면 assets/maps/custom.json)
//   G    — 16px 그리드 표시 토글
//   ESC  — 에디터 종료 → Title 화면 복귀
//
// JSON 스키마:
//   LoadMapFromJson / SaveMapToJson 은 메인 스펙 §4.7 와 동일한 키를 사용해
//   에디터에서 저장한 파일을 게임에서 그대로 로드할 수 있다.
//
// 출처:
//   - 메인 스펙 §4.7 Map schema.
//   - [4. Rendering.pdf] SDL2 사각형 채우기 / SDL_BLENDMODE_BLEND 투명 렌더.
//   - [3. Game Loop.pdf p.4] 독립 폴링 이벤트 루프.
// =============================================================================

#include "MapEditor.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// nlohmann/json — third_party/ 가 include 경로에 있으므로 직접 참조.
#include "json.hpp"

// DrawText 헬퍼 (src/render/Text.h).
#include "../render/Text.h"

namespace {

// =============================================================================
// 상수
// =============================================================================

constexpr int kScreenW   = 1280;
constexpr int kScreenH   = 720;
constexpr int kGridSize  = 16;   // 스냅 그리드 크기 (px).

// 기본 스폰 위치.
constexpr float kDefaultSpawnP1X = 160.0f;
constexpr float kDefaultSpawnP1Y = 560.0f;
constexpr float kDefaultSpawnP2X = 1120.0f;
constexpr float kDefaultSpawnP2Y = 560.0f;

// =============================================================================
// 오브젝트 타입 열거형
// =============================================================================

// 에디터에서 배치할 수 있는 오브젝트 종류.
// 키 1~8 에 대응한다.
enum class ObjType : int {
    StaticWall     = 0,  // 키 1 — 파괴 불가 정적 벽
    Destructible   = 1,  // 키 2 — 파괴 가능 타일
    BreakableSlab  = 2,  // 키 3 — 박격포 한 방에 무너지는 슬래브
    CoverSmall     = 3,  // 키 4 — 소형 엄폐물
    CoverLarge     = 4,  // 키 5 — 대형 엄폐물
    SpawnP1        = 5,  // 키 6 — P1 스폰 위치 (단일 포인트)
    SpawnP2        = 6,  // 키 7 — P2 스폰 위치 (단일 포인트)
    MovingPlatform = 7,  // 키 8 — 움직이는 발판
    kCount         = 8,
};

// 타입별 이름 (온스크린 UI 용).
const char* const kTypeNames[static_cast<int>(ObjType::kCount)] = {
    "1: Static Wall",
    "2: Destructible Tile",
    "3: Breakable Slab",
    "4: Cover (Small)",
    "5: Cover (Large)",
    "6: Spawn P1",
    "7: Spawn P2",
    "8: Moving Platform",
};

// 타입별 채우기 색상.
// Spawn 마커는 DrawSpawnMarker 에서 별도로 그린다.
SDL_Color TypeColor(ObjType t) {
    switch (t) {
        case ObjType::StaticWall:     return {90,  90,  90,  255};
        case ObjType::Destructible:   return {175, 140, 105, 255};
        case ObjType::BreakableSlab:  return {150, 120,  90, 255};
        case ObjType::CoverSmall:     return {100, 130, 160, 255};
        case ObjType::CoverLarge:     return {80,  110, 145, 255};
        case ObjType::SpawnP1:        return {220,  30,  30, 255};
        case ObjType::SpawnP2:        return {30,   80, 220, 255};
        case ObjType::MovingPlatform: return {60,  200, 200, 255};
        default:                      return {128, 128, 128, 255};
    }
}

// =============================================================================
// 배치된 오브젝트 데이터 구조체
// =============================================================================

// 사각형 오브젝트 한 개. Spawn 타입은 rect.w == rect.h == kGridSize 인
// 단일 셀로 저장한다.
struct PlacedRect {
    SDL_Rect rect;    // 화면 좌표 (px), 좌상단+크기 형식.
    ObjType  type;

    // BreakableSlab 전용 — hp / 색상.
    int   slabHp = 1;
    Uint8 slabR  = 150, slabG = 120, slabB = 90;

    // Cover 전용 — kind 문자열 ("small" | "large").
    // ObjType 에서 이미 구분하므로 런타임 문자열 비교는 저장 시에만 필요.

    // MovingPlatform 전용 — axis / amplitude / period (기본값 그대로).
    char  platAxis      = 'x';
    float platAmplitude = 120.0f;
    float platPeriod    = 3.0f;
};

// =============================================================================
// 에디터 상태
// =============================================================================

struct EditorState {
    // ---------- 배치된 오브젝트 ----------
    std::vector<PlacedRect> objects;

    // ---------- 스폰 위치 (항상 존재) ----------
    int spawnP1X = static_cast<int>(kDefaultSpawnP1X);
    int spawnP1Y = static_cast<int>(kDefaultSpawnP1Y);
    int spawnP2X = static_cast<int>(kDefaultSpawnP2X);
    int spawnP2Y = static_cast<int>(kDefaultSpawnP2Y);

    // ---------- UI 상태 ----------
    ObjType activeType   = ObjType::StaticWall;
    bool    showGrid     = true;

    // ---------- 드래그 상태 ----------
    bool    dragging     = false;
    int     dragStartX   = 0;
    int     dragStartY   = 0;

    // ---------- 현재 로드된 파일 경로 ----------
    // 비어 있으면 저장 시 "assets/maps/custom.json" 에 기록.
    std::string loadedPath;
    // 맵 이름 — 기존 맵 로드 시 보존, 새 맵/미로드 시 "custom".
    std::string mapName = "custom";
};

// =============================================================================
// 좌표 유틸리티
// =============================================================================

// 16px 그리드에 스냅 (내림 방향).
int SnapToGrid(int v) {
    // 음수 좌표에서도 올바르게 내림하기 위해 floor 계산.
    if (v >= 0) {
        return (v / kGridSize) * kGridSize;
    }
    return ((v - kGridSize + 1) / kGridSize) * kGridSize;
}

// 두 점으로 정규화된 SDL_Rect 생성 (좌상단 + 양수 크기).
SDL_Rect MakeRect(int x0, int y0, int x1, int y1) {
    int rx = std::min(x0, x1);
    int ry = std::min(y0, y1);
    int rw = std::abs(x1 - x0);
    int rh = std::abs(y1 - y0);
    return {rx, ry, rw, rh};
}

// 점이 사각형 안에 있는지 확인.
bool RectContains(const SDL_Rect& r, int px, int py) {
    return px >= r.x && px < r.x + r.w &&
           py >= r.y && py < r.y + r.h;
}

// =============================================================================
// 렌더링 헬퍼
// =============================================================================

// 단색 채우기 사각형.
void FillRect(SDL_Renderer* renderer, const SDL_Rect& r, SDL_Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(renderer, &r);
}

// 외곽선 사각형.
void DrawRect(SDL_Renderer* renderer, const SDL_Rect& r, SDL_Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderDrawRect(renderer, &r);
}

// 스폰 마커 — 작은 채워진 원 근사 (십자+다이아몬드) + 레이블.
void DrawSpawnMarker(SDL_Renderer* renderer, TTF_Font* font,
                     int cx, int cy, SDL_Color c, const char* label) {
    // 8px 반경 채워진 사각형으로 근사 (Circle 없이 간단하게).
    constexpr int kR = 8;
    SDL_Rect body = {cx - kR, cy - kR, kR * 2, kR * 2};
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 220);
    SDL_RenderFillRect(renderer, &body);

    // 외곽선.
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 180);
    SDL_RenderDrawRect(renderer, &body);

    // 레이블 텍스트 (폰트 없으면 생략).
    if (font) {
        DrawText(renderer, font, label,
                 cx, cy - kR - 14, {255, 255, 255, 230}, true);
    }
}

// =============================================================================
// 그리드 렌더링
// =============================================================================

void DrawGrid(SDL_Renderer* renderer) {
    // 밝기 낮은 선으로 16px 그리드를 표시.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 80, 80, 90, 60);

    for (int x = 0; x <= kScreenW; x += kGridSize) {
        SDL_RenderDrawLine(renderer, x, 0, x, kScreenH);
    }
    for (int y = 0; y <= kScreenH; y += kGridSize) {
        SDL_RenderDrawLine(renderer, 0, y, kScreenW, y);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// =============================================================================
// 오브젝트 렌더링
// =============================================================================

void DrawObjects(SDL_Renderer* renderer, TTF_Font* font,
                 const EditorState& state) {
    // 배치된 오브젝트를 타입 색상으로 채워 그린다.
    for (const auto& obj : state.objects) {
        SDL_Color c = (obj.type == ObjType::BreakableSlab)
                    ? SDL_Color{obj.slabR, obj.slabG, obj.slabB, 255}
                    : TypeColor(obj.type);
        FillRect(renderer, obj.rect, c);

        // 외곽선 — 인접 오브젝트와 경계를 구분.
        SDL_Color outline{
            static_cast<Uint8>(std::min(255, static_cast<int>(c.r) + 40)),
            static_cast<Uint8>(std::min(255, static_cast<int>(c.g) + 40)),
            static_cast<Uint8>(std::min(255, static_cast<int>(c.b) + 40)),
            180};
        DrawRect(renderer, obj.rect, outline);
    }

    // 스폰 마커.
    DrawSpawnMarker(renderer, font,
                    state.spawnP1X, state.spawnP1Y,
                    {220, 30, 30, 255}, "P1");
    DrawSpawnMarker(renderer, font,
                    state.spawnP2X, state.spawnP2Y,
                    {30, 80, 220, 255}, "P2");
}

// =============================================================================
// 드래그 미리보기 렌더링
// =============================================================================

void DrawDragPreview(SDL_Renderer* renderer,
                     const EditorState& state, int mouseX, int mouseY) {
    if (!state.dragging) return;

    // 현재 마우스 위치를 스냅해서 미리보기 rect 계산.
    int mx = SnapToGrid(mouseX);
    int my = SnapToGrid(mouseY);
    SDL_Rect r = MakeRect(state.dragStartX, state.dragStartY, mx, my);
    if (r.w == 0 || r.h == 0) return;

    // 타입 색에 알파 80 반투명으로 미리보기.
    SDL_Color c = TypeColor(state.activeType);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 80);
    SDL_RenderFillRect(renderer, &r);

    // 미리보기 외곽선.
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 200);
    SDL_RenderDrawRect(renderer, &r);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// =============================================================================
// HUD — 상단 도움말 패널
// =============================================================================

void DrawHUD(SDL_Renderer* renderer, TTF_Font* font,
             const EditorState& state, int mouseX, int mouseY) {
    if (!font) return;

    // 어두운 반투명 패널 배경.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 10, 12, 18, 200);
    SDL_Rect panel = {0, 0, kScreenW, 90};
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // 현재 활성 타입.
    char typeBuf[64];
    std::snprintf(typeBuf, sizeof(typeBuf),
                  "Type: %s",
                  kTypeNames[static_cast<int>(state.activeType)]);
    DrawText(renderer, font, typeBuf, 8, 6, {255, 220, 60, 255}, false);

    // 로드된 파일 경로 (또는 "(new map)").
    const std::string pathDisplay =
        state.loadedPath.empty() ? "(new map)" : state.loadedPath;
    char fileBuf[128];
    std::snprintf(fileBuf, sizeof(fileBuf), "File: %s", pathDisplay.c_str());
    DrawText(renderer, font, fileBuf, 8, 30, {180, 200, 220, 255}, false);

    // 마우스 스냅 좌표.
    int sx = SnapToGrid(mouseX);
    int sy = SnapToGrid(mouseY);
    char posBuf[48];
    std::snprintf(posBuf, sizeof(posBuf), "Cursor: (%d, %d)", sx, sy);
    DrawText(renderer, font, posBuf, 8, 54, {160, 180, 160, 255}, false);

    // 키 레전드 (오른쪽).
    const char* legend =
        "1-8:Type | LMB:Place | RMB:Delete | S:Save | N:New | F1/F2/F3:Load | G:Grid | ESC:Exit";
    DrawText(renderer, font, legend, kScreenW - 8, 6,
             {140, 140, 160, 255}, false);
    // 오른쪽 정렬을 위해 TextWidth 를 이용해 x 를 조정.
    int lw = TextWidth(font, legend);
    DrawText(renderer, font, legend,
             kScreenW - lw - 8, 6, {140, 140, 160, 255}, false);
}

// =============================================================================
// 맵 데이터 조작
// =============================================================================

// 기본 경계 벽을 가진 새 맵으로 상태를 초기화한다.
// 바닥, 천장, 좌/우 측벽의 4개 정적 벽을 배치한다.
void NewMap(EditorState& state) {
    state.objects.clear();
    state.loadedPath.clear();
    state.mapName = "custom";

    // 스폰 초기화.
    state.spawnP1X = static_cast<int>(kDefaultSpawnP1X);
    state.spawnP1Y = static_cast<int>(kDefaultSpawnP1Y);
    state.spawnP2X = static_cast<int>(kDefaultSpawnP2X);
    state.spawnP2Y = static_cast<int>(kDefaultSpawnP2Y);

    // 바닥 — y=704, 높이 16. (1280 × 16)
    PlacedRect floor;
    floor.rect = {0, 704, 1280, 16};
    floor.type = ObjType::StaticWall;
    state.objects.push_back(floor);

    // 천장 — y=0, 높이 16.
    PlacedRect ceiling;
    ceiling.rect = {0, 0, 1280, 16};
    ceiling.type = ObjType::StaticWall;
    state.objects.push_back(ceiling);

    // 좌측 벽 — x=0, 너비 16.
    PlacedRect leftWall;
    leftWall.rect = {0, 0, 16, 720};
    leftWall.type = ObjType::StaticWall;
    state.objects.push_back(leftWall);

    // 우측 벽 — x=1264, 너비 16.
    PlacedRect rightWall;
    rightWall.rect = {1264, 0, 16, 720};
    rightWall.type = ObjType::StaticWall;
    state.objects.push_back(rightWall);
}

// =============================================================================
// JSON 저장 / 로드
// =============================================================================

// 에디터 상태를 게임 호환 JSON 형식으로 파일에 저장한다.
// 저장 경로를 stdout 에 출력한다.
void SaveMap(const EditorState& state, const std::string& path) {
    using json = nlohmann::json;

    json j;
    j["name"]  = state.mapName;
    j["world"] = {{"width", 1280}, {"height", 720}};
    j["bgm"]   = "bgm_battle";

    // 스폰 위치.
    j["spawn_p1"] = {state.spawnP1X, state.spawnP1Y};
    j["spawn_p2"] = {state.spawnP2X, state.spawnP2Y};

    // tiles.static — 정적 벽.
    json staticArr  = json::array();
    // tiles.destructible — 파괴 가능 타일.
    json destructArr = json::array();
    // covers.
    json coversArr  = json::array();
    // breakable_slabs.
    json slabsArr   = json::array();
    // moving_platforms.
    json platArr    = json::array();

    for (const auto& obj : state.objects) {
        const SDL_Rect& r = obj.rect;
        // rect 는 [topLeftX, topLeftY, w, h] 형식.
        json rectArr = {r.x, r.y, r.w, r.h};

        switch (obj.type) {
            case ObjType::StaticWall:
                staticArr.push_back({{"rect", rectArr}});
                break;

            case ObjType::Destructible:
                destructArr.push_back({{"rect", rectArr}});
                break;

            case ObjType::BreakableSlab:
                slabsArr.push_back({
                    {"rect",  rectArr},
                    {"hp",    obj.slabHp},
                    {"color", {obj.slabR, obj.slabG, obj.slabB}}
                });
                break;

            case ObjType::CoverSmall:
                coversArr.push_back({
                    {"rect", rectArr},
                    {"kind", "small"}
                });
                break;

            case ObjType::CoverLarge:
                coversArr.push_back({
                    {"rect", rectArr},
                    {"kind", "large"}
                });
                break;

            case ObjType::MovingPlatform: {
                // axis 는 단일 문자 → 문자열 변환.
                char axisStr[2] = {obj.platAxis, '\0'};
                platArr.push_back({
                    {"rect",      rectArr},
                    {"axis",      std::string(axisStr)},
                    {"amplitude", obj.platAmplitude},
                    {"period",    obj.platPeriod}
                });
                break;
            }

            // Spawn 타입은 위에서 별도 처리됨.
            default:
                break;
        }
    }

    j["tiles"] = {
        {"static",      staticArr},
        {"destructible", destructArr}
    };
    if (!coversArr.empty())  j["covers"]           = coversArr;
    if (!slabsArr.empty())   j["breakable_slabs"]  = slabsArr;
    if (!platArr.empty())    j["moving_platforms"] = platArr;

    // 들여쓰기 2 칸 pretty-print 로 저장.
    std::ofstream ofs(path);
    if (!ofs) {
        std::fprintf(stderr, "[MapEditor] 저장 실패: %s\n", path.c_str());
        return;
    }
    ofs << j.dump(2) << "\n";
    std::printf("[MapEditor] 저장 완료: %s\n", path.c_str());
}

// JSON 파일을 에디터 상태로 불러온다.
// 파일이 없거나 파싱 실패 시 stderr 에 보고하고 상태를 변경하지 않는다.
void LoadMap(EditorState& state, const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::fprintf(stderr, "[MapEditor] 파일 열기 실패: %s\n", path.c_str());
        return;
    }

    nlohmann::json j;
    try {
        f >> j;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[MapEditor] JSON 파싱 실패 %s: %s\n",
                     path.c_str(), e.what());
        return;
    }

    // 파싱 성공 → 상태 초기화 후 채우기.
    state.objects.clear();
    state.loadedPath = path;
    // 기존 맵 이름 보존 (저장 시 그대로 사용). 없으면 "custom".
    state.mapName = j.value("name", std::string("custom"));

    // 스폰 위치.
    if (j.contains("spawn_p1") && j["spawn_p1"].size() >= 2) {
        state.spawnP1X = j["spawn_p1"][0].get<int>();
        state.spawnP1Y = j["spawn_p1"][1].get<int>();
    }
    if (j.contains("spawn_p2") && j["spawn_p2"].size() >= 2) {
        state.spawnP2X = j["spawn_p2"][0].get<int>();
        state.spawnP2Y = j["spawn_p2"][1].get<int>();
    }

    // tiles.static.
    if (j.contains("tiles") && j["tiles"].contains("static")) {
        for (const auto& s : j["tiles"]["static"]) {
            const auto& r = s["rect"];
            PlacedRect pr;
            pr.type = ObjType::StaticWall;
            pr.rect = {r[0].get<int>(), r[1].get<int>(),
                       r[2].get<int>(), r[3].get<int>()};
            state.objects.push_back(pr);
        }
    }

    // tiles.destructible.
    if (j.contains("tiles") && j["tiles"].contains("destructible")) {
        for (const auto& d : j["tiles"]["destructible"]) {
            const auto& r = d["rect"];
            PlacedRect pr;
            pr.type = ObjType::Destructible;
            pr.rect = {r[0].get<int>(), r[1].get<int>(),
                       r[2].get<int>(), r[3].get<int>()};
            state.objects.push_back(pr);
        }
    }

    // covers.
    if (j.contains("covers")) {
        for (const auto& c : j["covers"]) {
            const auto& r = c["rect"];
            PlacedRect pr;
            std::string kind = c.value("kind", std::string("small"));
            pr.type = (kind == "large") ? ObjType::CoverLarge : ObjType::CoverSmall;
            pr.rect = {r[0].get<int>(), r[1].get<int>(),
                       r[2].get<int>(), r[3].get<int>()};
            state.objects.push_back(pr);
        }
    }

    // breakable_slabs.
    if (j.contains("breakable_slabs")) {
        for (const auto& s : j["breakable_slabs"]) {
            const auto& r = s["rect"];
            PlacedRect pr;
            pr.type   = ObjType::BreakableSlab;
            pr.rect   = {r[0].get<int>(), r[1].get<int>(),
                         r[2].get<int>(), r[3].get<int>()};
            pr.slabHp = s.value("hp", 1);
            if (s.contains("color") && s["color"].size() >= 3) {
                pr.slabR = static_cast<Uint8>(s["color"][0].get<int>());
                pr.slabG = static_cast<Uint8>(s["color"][1].get<int>());
                pr.slabB = static_cast<Uint8>(s["color"][2].get<int>());
            }
            state.objects.push_back(pr);
        }
    }

    // moving_platforms.
    if (j.contains("moving_platforms")) {
        for (const auto& mp : j["moving_platforms"]) {
            const auto& r = mp["rect"];
            PlacedRect pr;
            pr.type         = ObjType::MovingPlatform;
            pr.rect         = {r[0].get<int>(), r[1].get<int>(),
                               r[2].get<int>(), r[3].get<int>()};
            std::string ax  = mp.value("axis", std::string("x"));
            pr.platAxis     = ax.empty() ? 'x' : ax[0];
            pr.platAmplitude = mp.value("amplitude", 120.0f);
            pr.platPeriod    = mp.value("period",      3.0f);
            state.objects.push_back(pr);
        }
    }

    std::printf("[MapEditor] 로드 완료: %s (%zu 오브젝트)\n",
                path.c_str(), state.objects.size());
}

// =============================================================================
// 오브젝트 배치 / 삭제
// =============================================================================

// 드래그로 확정된 사각형을 현재 타입으로 배치한다.
// Spawn 타입은 이 함수를 통하지 않고 PlaceSpawn 을 사용한다.
void PlaceRect(EditorState& state,
               int x0, int y0, int x1, int y1) {
    SDL_Rect r = MakeRect(x0, y0, x1, y1);
    // 면적 0 은 무시.
    if (r.w == 0 || r.h == 0) return;

    PlacedRect pr;
    pr.rect = r;
    pr.type = state.activeType;
    // BreakableSlab 기본값은 PlacedRect 초기값(1, 150,120,90) 그대로.
    // MovingPlatform 기본값도 초기값 그대로.
    state.objects.push_back(pr);
}

// Spawn 포인트를 단일 클릭 좌표로 갱신한다 (기존 스폰은 대체).
void PlaceSpawn(EditorState& state, int x, int y) {
    if (state.activeType == ObjType::SpawnP1) {
        state.spawnP1X = x;
        state.spawnP1Y = y;
    } else if (state.activeType == ObjType::SpawnP2) {
        state.spawnP2X = x;
        state.spawnP2Y = y;
    }
}

// 커서 아래 최상단 오브젝트를 삭제한다 (역순 탐색으로 가장 나중에 배치된 것 우선).
// Spawn 마커는 RMB 로 삭제하지 않는다 (항상 존재).
void DeleteAtCursor(EditorState& state, int mx, int my) {
    // 역순으로 탐색해 첫 번째 히트를 삭제.
    for (int i = static_cast<int>(state.objects.size()) - 1; i >= 0; --i) {
        if (RectContains(state.objects[i].rect, mx, my)) {
            state.objects.erase(state.objects.begin() + i);
            return;
        }
    }
}

}  // namespace (anonymous)

// =============================================================================
// 공개 진입점
// =============================================================================

void RunMapEditor(SDL_Window* /*window*/, SDL_Renderer* renderer, TTF_Font* font) {
    // ---- 상태 초기화 ----
    EditorState state;
    NewMap(state);   // 기본 경계 벽 4 개 + 기본 스폰 위치로 시작.

    // ---- 이벤트 루프 ----
    bool running = true;

    while (running) {
        // ----- 이벤트 처리 -----
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                // 창 닫기.
                case SDL_QUIT:
                    running = false;
                    break;

                // 키 입력.
                case SDL_KEYDOWN: {
                    const SDL_Scancode sc = ev.key.keysym.scancode;

                    // ESC — 에디터 종료.
                    if (sc == SDL_SCANCODE_ESCAPE) {
                        running = false;
                        break;
                    }

                    // 숫자 키 1~8 — 타입 선택.
                    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_8) {
                        int idx = sc - SDL_SCANCODE_1;
                        state.activeType = static_cast<ObjType>(idx);
                        break;
                    }

                    // F1 — Broken Bridge 로드.
                    if (sc == SDL_SCANCODE_F1) {
                        LoadMap(state, "assets/maps/broken_bridge.json");
                        break;
                    }
                    // F2 — Bunker 로드.
                    if (sc == SDL_SCANCODE_F2) {
                        LoadMap(state, "assets/maps/bunker.json");
                        break;
                    }
                    // F3 — Hideout 로드.
                    if (sc == SDL_SCANCODE_F3) {
                        LoadMap(state, "assets/maps/hideout.json");
                        break;
                    }

                    // S — 저장.
                    if (sc == SDL_SCANCODE_S) {
                        const std::string savePath =
                            state.loadedPath.empty()
                            ? "assets/maps/custom.json"
                            : state.loadedPath;
                        SaveMap(state, savePath);
                        // 저장한 경로를 현재 로드 경로로 설정
                        // (처음 저장 시 custom.json 이 loadedPath 로 설정됨).
                        state.loadedPath = savePath;
                        break;
                    }

                    // N — 새 맵.
                    if (sc == SDL_SCANCODE_N) {
                        NewMap(state);
                        break;
                    }

                    // G — 그리드 토글.
                    if (sc == SDL_SCANCODE_G) {
                        state.showGrid = !state.showGrid;
                        break;
                    }

                    break;
                }

                // 마우스 버튼 눌림.
                case SDL_MOUSEBUTTONDOWN: {
                    const int mx = ev.button.x;
                    const int my = ev.button.y;
                    const int sx = SnapToGrid(mx);
                    const int sy = SnapToGrid(my);

                    if (ev.button.button == SDL_BUTTON_LEFT) {
                        // Spawn 타입은 단일 클릭으로 즉시 배치.
                        if (state.activeType == ObjType::SpawnP1 ||
                            state.activeType == ObjType::SpawnP2) {
                            PlaceSpawn(state, sx, sy);
                        } else {
                            // 드래그 시작 — 스냅된 좌상단 좌표 저장.
                            state.dragging   = true;
                            state.dragStartX = sx;
                            state.dragStartY = sy;
                        }
                    } else if (ev.button.button == SDL_BUTTON_RIGHT) {
                        // RMB — 커서 아래 오브젝트 삭제.
                        DeleteAtCursor(state, mx, my);
                    }
                    break;
                }

                // 마우스 버튼 뗌.
                case SDL_MOUSEBUTTONUP: {
                    if (ev.button.button == SDL_BUTTON_LEFT && state.dragging) {
                        const int mx = ev.button.x;
                        const int my = ev.button.y;
                        const int ex = SnapToGrid(mx);
                        const int ey = SnapToGrid(my);

                        // 드래그 끝점이 시작점과 같으면 면적 0 → 배치 안 함.
                        if (ex != state.dragStartX || ey != state.dragStartY) {
                            PlaceRect(state,
                                      state.dragStartX, state.dragStartY,
                                      ex, ey);
                        }
                        state.dragging = false;
                    }
                    break;
                }

                default:
                    break;
            }
        }

        // ----- 렌더링 -----

        // 배경.
        SDL_SetRenderDrawColor(renderer, 30, 34, 40, 255);
        SDL_RenderClear(renderer);

        // 그리드 (토글).
        if (state.showGrid) DrawGrid(renderer);

        // 현재 마우스 위치 취득.
        int mouseX = 0, mouseY = 0;
        SDL_GetMouseState(&mouseX, &mouseY);

        // 배치된 오브젝트.
        DrawObjects(renderer, font, state);

        // 드래그 미리보기.
        DrawDragPreview(renderer, state, mouseX, mouseY);

        // HUD.
        DrawHUD(renderer, font, state, mouseX, mouseY);

        SDL_RenderPresent(renderer);

        // ~60 fps 타겟 (16 ms sleep).
        SDL_Delay(16);
    }
}
