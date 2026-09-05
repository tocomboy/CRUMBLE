// =============================================================================
// src/main.cpp — Crumble 진입점 (P3 MVP).
//
// 본 파일의 누적 갱신 이력:
//   - P0 Task 0.3:  최소 SDL 창 + cleanup.
//   - P0 Task 0.5:  fixed-timestep accumulator + FPS 타이틀.
//   - P1 Task 1.6:  PhysicsWorld + 5 boxes stacking (시각 검증, 폐기).
//   - P2 Task 2.3:  InputManager + 2 Player + ContactCallback 라우팅.
//   - P3 Task 3.6:  🎯 MVP — GameFlow / Map / ProjectilePool / Rifle / HUD
//                   를 모두 묶어 발표용 1v1 매치 완성.
//
// 메인 루프 한 단계 (한 fixed-step) 의 흐름:
//   1) input.BeginFrame / PumpFromSDL.
//   2) flow.Update(dt) — 자동 전이 진행.
//   3) IsCombatActive 일 때만 (Playing 상태):
//      a) p1.UpdateInput / p2.UpdateInput.
//      b) Fire 입력 → rifle.TryFire (방향 = 마지막 vel.x 부호).
//      c) rifle1/rifle2.Update (재장전 진행).
//      d) world.Step (충돌 검출 + 응답 + ContactCallback).
//      e) projectiles.Update (lifetime + expired 정리).
//      f) p1/p2.UpdateFSM.
//      g) Fall-death 검사 (HP/2 + Respawn).
//      h) HP=0 검사 → flow.OnRoundOver.
//
// ContactCallback 의 책무:
//   - 플레이어 ground/wall 검출 (Player.EvaluateContact).
//   - 투사체-플레이어 데미지 (자기 owner 무시 + expired = true 로 deferred 파괴).
//
// 결정론 / 스레드:
//   - 메인 스레드 단독.
//   - 모든 게임 로직이 fixed-timestep 안에서 동일 dt = 1/60 로 호출.
//   - 발사 방향이 vel.x 부호 → 같은 입력에 같은 결과.
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 3 Task 3.6.
//   - [3. Game Loop.pdf p.2-4, p.7-8] SDL setup + Fixed Timestep.
//   - [5. Input Handling.pdf p.4-8] 하이브리드 입력 (PollEvent + GetKeyboardState).
//   - [4. Rendering.pdf] Rect / Color / Present 패턴.
// =============================================================================

#include <SDL.h>
#include <SDL_image.h>     // P9 — IMGGuard (IMG_INIT_PNG 의존).
#include <SDL_mixer.h>     // P9 — MixGuard.
#include <SDL_ttf.h>       // P9 — TTFGuard / Text 렌더.

#include <algorithm>
#include <atomic>    // P9 — assetsReady (worker thread 가 set, main 이 read).
#include <cmath>     // std::sqrt — DrawFilledCircle.
#include <cstdio>
#include <cstdlib>   // std::rand — F9 stress spawn.
#include <fstream>   // P10 — F12 snapshot 파일 쓰기.
#include <memory>    // P9 — std::unique_ptr (MovingPlatform / MixGuard 소유권).
#include <random>    // P9 — std::mt19937 (MapPool 셔플).
#include <stdexcept> // P9 — JSON 로드 실패 catch.
#include <string>
#include <vector>

// P10 — F12 async snapshot 직렬화 (debug build only).
//   third_party/json.hpp 는 nlohmann/json single-header — Map.cpp 와 동일 경로.
//   CRUMBLE_DEBUG 가드 안에서만 사용하지만 include 자체는 밖에 둬도 무방.
#ifdef CRUMBLE_DEBUG
// third_party/ 가 CMakeLists 의 target_include_directories 에 포함되어
// json.hpp 를 직접 참조 가능 (Map.cpp 의 "../../third_party/json.hpp" 와 달리
// main.cpp 는 src/ 밖에서 컴파일되므로 include path 기준 경로 사용).
#include "json.hpp"
#endif

#include "core/Timer.h"
#include "flow/GameFlow.h"
#include "game/BreakableSlab.h"
#include "game/ChunkSpawner.h"
#include "game/CombatHooks.h"
#include "game/Map.h"
#include "game/MapEditor.h"
#include "game/MovingPlatform.h"
#include "game/Player.h"
#include "game/Projectile.h"
#include "game/TileGrid.h"
#include "game/Weapon.h"
#include "game/weapons/Mortar.h"
#include "game/weapons/MortarAimState.h"
#include "game/weapons/Rifle.h"
#include "game/weapons/Shotgun.h"
#include "input/Bindings.h"
#include "input/InputManager.h"
#include "audio/AudioManager.h"          // P9 Task 9.3.
#include "math/MathUtils.h"             // P10-B — SmoothStep, Lerp, Ease::OutBounce 등.
#include "math/Vec2.h"
#include "physics/DebugDraw.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsWorld.h"
#include "render/HUD.h"
#include "render/RenderUtils.h"          // P10-B — LerpColor (폭발 그라디언트).
#include "render/Text.h"                 // P9 Task 9.4.
#include "resource/AsyncLoader.h"        // P9 Task 9.2.
#include "resource/ResourceManager.h"    // P9 Task 9.2.
#include "resource/SDLGuards.h"          // P9 Task 9.2.
#include "resource/ThreadPool.h"         // P10 Task 10 — F12 async snapshot.

namespace {

// 화면 / 시뮬레이션 상수. 변경 시 다른 모듈에도 영향 가능 — 단일 정의.
constexpr int   kScreenW  = 1280;
constexpr int   kScreenH  = 720;
constexpr float kFixedDt  = 1.0f / 60.0f;
constexpr float kMaxFt    = 0.25f;       // [3. Game Loop.pdf p.7] spiral-of-death 안전망.
constexpr float kFallY    = static_cast<float>(kScreenH) + 200.0f;  // 추락 임계.

// ExplosionEffect (P8 시각 튜닝): 박격포 폭발 시 시각적 잔상.
//   - 폭발 중심에서 확장하는 원 + 시간에 따른 색/알파 페이드아웃.
//   - 시뮬레이션 외부 (PhysicsWorld 변경 없음) → 결정론 / replay 영향 0.
//   - 한 폭발 = 한 effect. main.cpp 의 매 fixed-step 에서 lifetime 진행 후
//     만료 시 vector 에서 제거.
struct ExplosionEffect {
    Vec2  center;
    float lifetime    = 0.0f;
    float maxLifetime = 0.4f;     // 0.4 s 동안 빛 → 검정 페이드.
};

// SDL2 가 native 채워진 원을 제공하지 않아 horizontal scanline 으로 근사.
//   - 가로 line 한 줄 = SDL_RenderDrawLine 한 번. 반경 100 정도면 200 line.
//   - alpha blending 은 호출자가 SDL_SetRenderDrawBlendMode(SDL_BLENDMODE_BLEND)
//     먼저 설정. 색은 SDL_SetRenderDrawColor 로 미리.
void DrawFilledCircle(SDL_Renderer* r, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
        const int dx = static_cast<int>(std::sqrt(static_cast<float>(
            radius * radius - dy * dy)));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

// AABB body 한 개를 사각형으로 채워 그리는 헬퍼.
void DrawAabb(SDL_Renderer* r, const PhysicsBody& b, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rect = {
        static_cast<int>(b.pos.x - b.half.x),
        static_cast<int>(b.pos.y - b.half.y),
        static_cast<int>(b.half.x * 2.0f),
        static_cast<int>(b.half.y * 2.0f),
    };
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderFillRect(r, &rect);
}

// (P3 결함 fix #7) Projectile 이 AABB shape 으로 정렬되어 별도 Circle 헬퍼
// 불필요. 투사체 렌더는 DrawAabb 한 가지로 통일.

// PlayerWeapons (P5): 한 플레이어가 들고다니는 3 종 무기 + 현재 선택 인덱스.
//   - 1/2/3 키로 currentIdx 갱신 → Current() 가 활성 무기 포인터를 반환.
//   - 본 구조는 stack-allocated 한 무기 인스턴스 (main 함수 안의 지역 변수)
//     의 포인터만 들고다닌다 — 소유권 없음.
struct PlayerWeapons {
    Rifle*   rifle   = nullptr;
    Shotgun* shotgun = nullptr;
    Mortar*  mortar  = nullptr;
    int      currentIdx = 0;        // 0 = Rifle, 1 = Shotgun, 2 = Mortar.

    // 현재 활성 무기 포인터. nullptr 은 호출자 책임 (본 P5 단계에서는 항상
    // 세 무기 모두 인스턴스화 후 wire 하므로 null 발생 안 함).
    Weapon* Current() {
        if (currentIdx == 0) return rifle;
        if (currentIdx == 1) return shotgun;
        return mortar;
    }
};

// GameContext: 메인 루프가 들고다니는 모든 게임 객체의 묶음.
//   - main 함수 안의 지역 변수 묶음을 struct 화 — 콜백 람다 / 헬퍼 함수에서
//     "이 변수들 다 캡처해야지" 가 아니라 "ctx 만 받으면 된다" 로 단순화.
struct GameContext {
    PhysicsWorld          world;
    InputManager          input;
    ProjectilePool        projectiles;
    GameFlow              flow;
    MapData               map;
    std::vector<BodyId>   staticTileIds;
    Player*               p1       = nullptr;
    Player*               p2       = nullptr;
    PlayerWeapons         pw1;       // P5 — 3 종 무기 인스턴스 포인터 묶음.
    PlayerWeapons         pw2;
    // P9 — 움직이는 발판 소유권 vector. P6 시점의 단일 stack 인스턴스에서
    // 라운드별 맵 셔플에 따라 0~N 개로 가변되도록 unique_ptr 벡터로 변경.
    // TearDownMap 시 body 파괴 후 vector clear → 다음 LoadMap 이 새로 채움.
    std::vector<std::unique_ptr<MovingPlatform>> platforms;

    // P8 — destructible 타일 격자 + chunk 풀. 둘 다 main 함수의 stack-allocated
    // 인스턴스 포인터 (소유권 없음). TileGrid 는 PhysicsWorld& 를 ctor 에서
    // 받아야 하므로 GameContext 멤버로 직접 두기 어려워 포인터 형태.
    TileGrid*     tiles  = nullptr;
    ChunkSpawner* chunks = nullptr;
    // 한 방에 무너지는 긴 벽/지붕 슬래브 (소유권 없음 — main stack 인스턴스).
    BreakableSlabField* slabs = nullptr;

    // P8 시각 튜닝 — 폭발 시각 효과 vector. 한 폭발 = 한 entry. 매 fixed-step
    // 마다 lifetime 진행 + 만료 시 swap-pop 으로 제거. 결정론 / 시뮬레이션 무관.
    std::vector<ExplosionEffect> explosions;
};

// MakeStaticBody: 정적 AABB 한 덩어리를 PhysicsBody def 로 변환.
//   - LoadMap 의 staticBlocks / covers 양쪽이 동일 형식 → 헬퍼로 분리.
PhysicsBody MakeStaticBody(Vec2 center, Vec2 half) {
    PhysicsBody def;
    def.type  = BodyType::Static;
    def.SetMass(0.0f);
    def.shape = ShapeType::AABB;
    def.pos   = center;
    def.half  = half;
    // P8 collision layer wiring — static terrain (bit 3). mask 는 모든 layer
    // 와 충돌 (~0u). TileGrid 의 정적 타일도 동일 layer 비트.
    def.collisionLayer = 1u << 3;
    def.collisionMask  = ~0u;
    return def;
}

// LoadMap: MapData 한 장을 PhysicsWorld + TileGrid + MovingPlatform 으로 펼침.
//   - P3 단계: staticBlocks 만 처리.
//   - P9 단계 (현재): destructibleAreas (TileGrid.LoadDestructibleArea), covers
//     (정적 블록 동등), movingPlatforms (Kinematic 진동) 까지 모두 데이터-드리븐.
//   - 호출자는 본 함수 전에 ctx.tiles / ctx.chunks 가 valid pointer 여야 한다.
//   - 라운드 사이 호출되므로 항상 MarkStaticDirty.
void LoadMap(GameContext& ctx, const MapData& m) {
    ctx.map = m;

    // 1) 정적 블록 (바닥 / 벽 / 천장).
    ctx.staticTileIds.clear();
    for (const auto& s : m.staticBlocks) {
        ctx.staticTileIds.push_back(ctx.world.CreateBody(
            MakeStaticBody(s.center, s.half)));
    }
    // 2) Cover (현재 P9 단계에서는 정적 블록과 같은 PhysicsBody 표현).
    for (const auto& c : m.covers) {
        ctx.staticTileIds.push_back(ctx.world.CreateBody(
            MakeStaticBody(c.center, c.half)));
    }

    // 3) Destructible 영역 — TileGrid 가 펼침.
    if (ctx.tiles) {
        for (const auto& d : m.destructibleAreas) {
            ctx.tiles->LoadDestructibleArea(d.topLeft, d.size);
        }
    }

    // 3.5) Breakable slabs — 박격포 폭발 시 통째로 무너지는 긴 벽/지붕.
    //      라운드마다 LoadMap 이 새로 호출되므로 Load 만으로 라운드 리셋 시
    //      손상 없는 fresh 상태가 보장된다 (TearDownMap 의 Clear 와 짝).
    if (ctx.slabs) ctx.slabs->Load(ctx.world, m.slabs);

    // 4) Moving platforms — Kinematic 진동 발판. 각 정의마다 unique_ptr 인스턴스.
    for (const auto& mp : m.movingPlatforms) {
        const PlatformAxis ax = (mp.axis == 'y' || mp.axis == 'Y')
                              ? PlatformAxis::Y : PlatformAxis::X;
        ctx.platforms.emplace_back(std::make_unique<MovingPlatform>(
            ctx.world, mp.center, mp.half, ax, mp.amplitude, mp.period));
    }

    // 5) P7 staticGrid_ rebuild 트리거.
    ctx.world.MarkStaticDirty();
}

// TearDownMap: LoadMap 이 만든 모든 per-map 객체를 PhysicsWorld 에서 제거.
//   - 라운드 사이 맵 셔플 시 호출. Player body 와 ProjectilePool 은 건드리지
//     않는다 (Player 는 Respawn 으로 위치만 리셋, projectiles 는 expired 자연
//     정리).
//   - 호출 순서: Static body → Destructible 타일 → MovingPlatform → Chunks.
void TearDownMap(GameContext& ctx) {
    // 정적 body 들 (staticBlocks + covers).
    for (BodyId id : ctx.staticTileIds) ctx.world.DestroyBody(id);
    ctx.staticTileIds.clear();

    // Destructible 타일 (TileGrid).
    if (ctx.tiles) ctx.tiles->Clear();

    // Breakable slabs — body 제거 + 내부 상태 초기화.
    if (ctx.slabs) ctx.slabs->Clear(ctx.world);

    // Chunks (Active 잔해 — 다음 라운드에 떠다니지 않게).
    if (ctx.chunks) ctx.chunks->Reset();

    // Moving platforms — body 파괴 후 vector clear (unique_ptr 자동 소멸).
    for (auto& p : ctx.platforms) {
        if (p) ctx.world.DestroyBody(p->GetBodyId());
    }
    ctx.platforms.clear();

    // 시각 효과 (폭발 잔상) 도 정리 — 다음 라운드 시작 시 깨끗한 화면.
    ctx.explosions.clear();

    ctx.world.MarkStaticDirty();
}

// MapPool: 3 종 맵 + 라운드별 셔플.
//   - 메인 스펙 §4 — 라운드 1~3 은 결정론 셔플 (한 매치 당 한 번 섞고 그
//     순서로 사용), 라운드 4~5 (있다면) 는 std::rand 순수 무작위.
//   - 결정론: shuffle_ 의 std::mt19937 seed 는 std::random_device — 매치마다
//     순서가 다름. P4 의 byte-equal replay 테스트는 본 무작위와 무관 (PhysicsWorld
//     단독 테스트). 발표 시연에서 매치마다 다른 맵 순서 = 다양성 의도.
struct MapPool {
    std::vector<MapData> maps;
    std::vector<int>     shuffled;   // round 1~3 의 인덱스.

    // 3 종 JSON 맵 로드 + 라운드 1~3 순서 셔플.
    void Init() {
        try {
            maps.push_back(LoadMapFromJson("assets/maps/broken_bridge.json"));
            maps.push_back(LoadMapFromJson("assets/maps/bunker.json"));
            maps.push_back(LoadMapFromJson("assets/maps/hideout.json"));
        } catch (const std::exception& e) {
            // JSON 로드 실패 — 회귀 가드 dev arena 한 장으로 플레이는 계속 가능.
            std::fprintf(stderr,
                         "MapPool::Init: JSON load failed, using DevArena fallback: %s\n",
                         e.what());
            maps.clear();
            maps.push_back(MakeHardcodedDevMap());
        }
        shuffled.clear();
        for (int i = 0; i < static_cast<int>(maps.size()); ++i) shuffled.push_back(i);
        std::shuffle(shuffled.begin(), shuffled.end(),
                     std::mt19937(std::random_device{}()));
    }

    // round (1-base) 에 사용할 맵 반환.
    //   - round ≤ 3: shuffled[round-1] 결정론 순서 (매치 시작 시 한 번 섞음).
    //   - round ≥ 4: std::rand() 순수 무작위 (best-of-3 over 5 → 4-5 라운드는
    //     매치 동점 시에만 진입).
    const MapData& MapForRound(int round) const {
        if (maps.empty()) {
            static MapData empty = MakeHardcodedDevMap();
            return empty;
        }
        if (round >= 1 && round <= static_cast<int>(shuffled.size())) {
            return maps[shuffled[round - 1]];
        }
        return maps[std::rand() % maps.size()];
    }
};

// StartRound: 라운드 시작 시 호출. 맵 셔플 + HP 리셋 + 스폰 위치 복귀.
//   - GameFlow 가 RoundIntro 진입 시 main 측에서 호출.
//   - P9 갱신: 라운드별 다른 맵을 사용 (메인 스펙 §4 — 1~3 결정론 셔플,
//     4~5 무작위). TearDownMap → LoadMap(MapForRound) 순서.
//   - Player 의 spawn 위치는 새 맵의 spawnP1/P2 로 갱신.
void StartRound(GameContext& ctx, const MapPool& pool) {
    // 1) 이전 라운드의 맵 객체 정리 + 새 맵 로드.
    TearDownMap(ctx);
    LoadMap(ctx, pool.MapForRound(ctx.flow.Round()));

    // 2) Player 의 spawn 위치를 새 맵에 맞춰 갱신 — PlayerStats.spawnPoint 가
    //    Respawn() 의 위치 source. P9 의 라운드별 맵 셔플 시 매번 갱신.
    if (ctx.p1) ctx.p1->Stats().spawnPoint = ctx.map.spawnP1;
    if (ctx.p2) ctx.p2->Stats().spawnPoint = ctx.map.spawnP2;

    // 3) HP / alive / hitFlashTimer 리셋 + Respawn.
    //   hitFlashTimer: 라운드 전환 시 이전 라운드의 잔여 플래시가 남지 않도록 0 초기화.
    if (ctx.p1) {
        ctx.p1->Stats().hp             = ctx.p1->Stats().maxHp;
        ctx.p1->Stats().alive          = true;
        ctx.p1->Stats().hitFlashTimer  = 0.0f;
        ctx.p1->Respawn();
    }
    if (ctx.p2) {
        ctx.p2->Stats().hp             = ctx.p2->Stats().maxHp;
        ctx.p2->Stats().alive          = true;
        ctx.p2->Stats().hitFlashTimer  = 0.0f;
        ctx.p2->Respawn();
    }
    // 메인 스펙 §3.4 — 라운드 사이 지형 손상 비-지속. LoadMap 이 새 destructible
    // 영역을 fresh 상태로 만들었으므로 별도 RestoreAll 불필요. (TearDownMap 이
    // 이전 타일을 모두 destroy.)
}

}  // namespace

int main(int /*argc*/, char* /*argv*/[]) try {
    // Windows(SDL_MAIN_HANDLED) 에서 SDL 진입점을 직접 관리한다고 알린다.
    // SDL_Init 보다 먼저 호출해야 하며, SDL_MAIN_HANDLED 미정의(Linux) 환경에서도
    // 안전한 호출이라 무조건 부른다 (cross-platform 일원화).
    SDL_SetMainReady();

    // 오디오 지연(latency) 완화 — WSL/Linux 의 PulseAudio 는 기본 버퍼가 커서
    // SFX 가 1~2 초 늦게 들리는 경우가 있다. 다만 목표 지연을 너무 낮게(예: 60ms)
    // 잡으면 WSLg 의 RDP 오디오 파이프가 버퍼를 제때 못 채워 underrun → "몇 초마다
    // 몰아서 들리는" 토막 재생이 발생한다. 그래서 underrun 이 나지 않는 안정 구간
    // (100ms) 으로 잡아 기본값(수백 ms)보다는 크게 낮추되 토막 현상은 피한다.
    // 아래 chunksize(2048) 와 함께 ~2x 버퍼 헤드룸을 확보한다. Windows(WASAPI)
    // 등 비-Pulse 환경에서는 무시되므로 안전. (Mix_OpenAudio 보다 반드시 먼저
    // 설정해야 효과가 있다.)
    SDL_setenv("PULSE_LATENCY_MSEC", "100", 1);

    // ===== 1) SDL 초기화 (P9 — RAII guards) ==================================
    //   guards 의 소멸 순서가 init 의 역순이라 main 종료 시 자동 cleanup.
    //   SDL_INIT_AUDIO 가 SDLGuard 단계에서 실패 시 throw → catch 로 종료.
    SDLGuard sdl(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
    IMGGuard img(IMG_INIT_PNG);   // P9 Task 9.4 — 향후 PNG 자산 (현 단계 미사용).
    TTFGuard ttf;                 // P9 Task 9.4 — 텍스트 렌더 의존.

    // Mix_OpenAudio 는 일부 환경 (헤드리스 / 오디오 디바이스 부재) 에서 실패
    // 가능. 본 게임은 audio 없이도 정상 진행해야 하므로 unique_ptr 로 감싸
    // 실패 시 nullopt 상태로 계속.
    std::unique_ptr<MixGuard> mixGuard;
    try {
        mixGuard = std::make_unique<MixGuard>(44100, 2, 2048);  // 2048 ≈ 46ms — underrun 안전 구간(토막 재생 방지).
    } catch (const std::exception& e) {
        std::fprintf(stderr,
                     "[main] audio init failed (continuing silent): %s\n",
                     e.what());
    }

    SDL_Window* window = SDL_CreateWindow(
        "Crumble",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kScreenW, kScreenH,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return 1;
    }

    // 창 크기 조절 시 16:9 비율 유지. 논리 해상도를 1280x720 으로 고정하면 SDL 이
    // 모든 창 크기에서 자동으로 스케일링하고, 남는 영역은 레터박스(검은 띠)로 채운다.
    // 마우스 좌표(SDL_GetMouseState 등)도 논리 좌표로 자동 변환되므로 맵 에디터의
    // 클릭 위치가 어떤 창 크기에서도 정확히 일치한다.
    SDL_RenderSetLogicalSize(renderer, kScreenW, kScreenH);

    // ===== P10 — ThreadPool (debug build only) ==============================
    //   F12 스냅샷 직렬화 + 파일 쓰기를 메인 스레드에서 분리하기 위해 사용.
    //   worker 수: hardware_concurrency 와 2 중 큰 값 (헤드리스 환경 보정).
    //   CRUMBLE_DEBUG 가드 안에서만 컴파일 — Release 빌드에서는 인스턴스 자체가 없음.
#ifdef CRUMBLE_DEBUG
    ThreadPool threadPool(std::max(2,
        static_cast<int>(std::thread::hardware_concurrency())));
#endif

    // ===== P9 — ResourceManager + AsyncLoader + AudioManager ================
    //   resources : Mix_Chunk / Mix_Music / TTF_Font 캐시 (RAII 보유).
    //   loader    : 단일 worker thread. Title 화면 진입 직후 자산 로드 비동기 진행.
    //   audio     : resources 의 Get* 를 받아 Mix_Play* 호출. 캐시 miss 는 무음.
    //   assetsReady: worker 가 끝나면 true 로 set. main 이 polling.
    //                std::atomic<bool> — 두 스레드 사이의 release/acquire 보장.
    ResourceManager      resources;
    AsyncLoader          loader;
    AudioManager         audio(resources);
    std::atomic<bool>    assetsReady{false};
    bool                 titleBgmStarted = false;   // assetsReady 직후 한 번만 재생.

    // 자산 로드 잡 — Title 화면 진입 직후 worker 가 disk IO 진행.
    //   - Mix_LoadWAV / Mix_LoadMUS 가 nullptr 이어도 ResourceManager 는 안전.
    //   - 폰트는 실 사용 (Title / Controls / RoundIntro / MatchEnd / HUD score).
    //   - 본 jobs 안에서 PhysicsWorld 에 절대 접근 안 함 → 결정론 비협상 불변식 유지.
    loader.Enqueue([&] {
        // Fonts — Ubuntu Regular (28 / 22) + Bold (64).
        resources.LoadFont("regular", "assets/fonts/Ubuntu-Regular.ttf", 28);
        resources.LoadFont("hud",     "assets/fonts/Ubuntu-Regular.ttf", 22);
        resources.LoadFont("title",   "assets/fonts/Ubuntu-Bold.ttf",    64);

        // SFX — 발표 직전에 채워질 자산. 미스 시 무음 (PlaySfx 가 nullptr return).
        resources.LoadSfx("rifle",        "assets/sfx/sfx_rifle.wav");
        resources.LoadSfx("shotgun",      "assets/sfx/sfx_shotgun.wav");
        resources.LoadSfx("mortar_fire",  "assets/sfx/sfx_mortar_fire.wav");
        resources.LoadSfx("mortar_blast", "assets/sfx/sfx_mortar_blast.wav");
        resources.LoadSfx("hit",          "assets/sfx/sfx_hit.wav");
        resources.LoadSfx("jump",         "assets/sfx/sfx_jump.wav");
        resources.LoadSfx("land",         "assets/sfx/sfx_land.wav");
        resources.LoadSfx("swap",         "assets/sfx/sfx_swap.wav");
        resources.LoadSfx("select",       "assets/sfx/sfx_select.wav");
        resources.LoadSfx("round",        "assets/sfx/sfx_round.wav");

        // BGM — Title / Battle 두 곡.
        resources.LoadBgm("bgm_title",    "assets/bgm/bgm_title.ogg");
        resources.LoadBgm("bgm_battle",   "assets/bgm/bgm_battle.ogg");

        // 모든 잡이 끝났음을 main 에게 알림. release fence 로 위 cache 변경이
        // assetsReady=true 보다 먼저 보이게 보장.
        assetsReady.store(true, std::memory_order_release);
    });

    // ===== 스프라이트 텍스처 (메인 스레드 동기 로드) ========================
    //   캐릭터 / 무기 PNG 는 GPU 텍스처라 renderer 를 소유한 메인 스레드에서만
    //   생성 가능 — worker 의 loader.Enqueue 안에서 IMG_LoadTexture 를 부르면
    //   안 된다 (renderer 비-thread-safe). 작은 파일이라 동기 로드해도 체감
    //   지연 없음. 파일 부재 시 LoadTexture 가 nullptr 반환 → 렌더 시 기존
    //   단색 AABB fallback (자산 부재 정책 유지).
    //   인덱스: [0]=P1(red), [1]=P2(blue) / 무기 [0]=rifle [1]=shotgun [2]=mortar.
    SDL_Texture* texPlayer[2] = {
        resources.LoadTexture("player_red",  "assets/sprites/player_red.png",  renderer),
        resources.LoadTexture("player_blue", "assets/sprites/player_blue.png", renderer),
    };
    SDL_Texture* texWeapon[3] = {
        resources.LoadTexture("weapon_rifle",   "assets/sprites/weapon_rifle.png",   renderer),
        resources.LoadTexture("weapon_shotgun", "assets/sprites/weapon_shotgun.png", renderer),
        resources.LoadTexture("weapon_mortar",  "assets/sprites/weapon_mortar.png",  renderer),
    };
    // 무기별 grip 픽셀 (sprites_manifest.json) — 손에 정렬할 기준점.
    //   rifle(18,7) / shotgun(16,8) / mortar(14,26).
    static const int kWeaponGripX[3] = {18, 16, 14};
    static const int kWeaponGripY[3] = { 7,  8, 26};
    // 플레이어별 run-cycle 애니메이션 위상 (이동 중 누적, 정지 시 0).
    float spriteAnim[2] = {0.0f, 0.0f};

    // ===== 2) 게임 컨텍스트 구성 ============================================
    GameContext ctx;
    BindDefaultActions(ctx.input);
    ctx.projectiles.Init(ctx.world, 100);   // 풀 capacity 100 (양 측 합산).

    // P9 — TileGrid + ChunkSpawner 를 LoadMap 호출 전에 먼저 준비. LoadMap 이
    // map 의 destructibleAreas 를 TileGrid 로 펼치므로 본 두 객체가 valid 해야 한다.
    TileGrid     tiles(ctx.world);
    ChunkSpawner chunks;
    chunks.Init(ctx.world, /*capacity*/ 256);   // 24 타일 × 6 × 1.5 안전 마진.
    BreakableSlabField slabs;
    ctx.tiles  = &tiles;
    ctx.chunks = &chunks;
    ctx.slabs  = &slabs;

    // P9 — 3 종 JSON 맵 로드 + 라운드 셔플. 실패 시 DevArena 회귀 가드.
    MapPool mapPool;
    mapPool.Init();
    // 초기 맵 — Title 화면에서도 시각적으로 보이게 round 1 의 맵을 미리 로드.
    LoadMap(ctx, mapPool.MapForRound(1));

    // 두 플레이어 — 맵의 스폰 좌표 사용.
    Player p1(ctx.world, ctx.input, "p1", ctx.map.spawnP1);
    Player p2(ctx.world, ctx.input, "p2", ctx.map.spawnP2);
    ctx.p1 = &p1;
    ctx.p2 = &p2;

    // 두 플레이어의 3 종 무기 인스턴스 (P5).
    //   - owner index 0 / 1 — friendly fire 방지 + 투사체 색 구분.
    //   - stack-allocated. 수명은 main 함수 끝까지 유지.
    Rifle   p1Rifle  (ctx.projectiles, 0);
    Shotgun p1Shotgun(ctx.projectiles, 0);
    Mortar  p1Mortar (ctx.projectiles, 0);
    Rifle   p2Rifle  (ctx.projectiles, 1);
    Shotgun p2Shotgun(ctx.projectiles, 1);
    Mortar  p2Mortar (ctx.projectiles, 1);

    // PlayerWeapons 에 포인터 wire — 이후 코드는 ctx.pw1/pw2 로 통일.
    ctx.pw1.rifle   = &p1Rifle;
    ctx.pw1.shotgun = &p1Shotgun;
    ctx.pw1.mortar  = &p1Mortar;
    ctx.pw2.rifle   = &p2Rifle;
    ctx.pw2.shotgun = &p2Shotgun;
    ctx.pw2.mortar  = &p2Mortar;

    // Player ↔ Mortar 양방향 wire — Mortar 의 aim_.facingSign 자동 동기.
    p1.SetMortarRef(&p1Mortar);
    p2.SetMortarRef(&p2Mortar);

    // 초기 facing — P1 은 우측, P2 는 좌측 (마주보게). SetFacingSign 이
    // mortar_->Aim().facingSign 도 함께 갱신.
    p1.SetFacingSign(+1);
    p2.SetFacingSign(-1);

    // P9 — TileGrid / ChunkSpawner / MovingPlatform 모두 LoadMap 이 데이터-드리븐
    // 으로 펼침. 본 자리는 P8 까지 하드코딩 셋업이 있던 위치. 이전: tiles.LoadDestructibleArea
    // 호출 + 단일 stack-allocated MovingPlatform plat0 push_back. 현재: MapPool.Init →
    // LoadMap → ctx.platforms 가 자동 채워짐.

    // P4 디버그 오버레이 옵션 — F1/F2/F3 토글로 켜고 끔.
    DebugDrawOptions debugOpts;

#ifdef CRUMBLE_DEBUG
    // F8 런타임 튜닝 파라미터 (debug build only).
    //   - Tab 키로 현재 파라미터 순환, Left/Right 로 값 조정.
    //   - 조정 값은 PhysicsWorld / Player 에 즉시 반영.
    //
    // 튜닝 파라미터 열거형.
    //   Gravity      : 중력 가속도 (y 성분). 기본 980 px/s².
    //   Restitution  : 전체 dynamic body 의 기본 반발 계수. 0 (완전비탄성) ~ 1 (완전탄성).
    //   PlayerSpeed  : 플레이어 이동 속도 배율.
    enum class TuningParam { Gravity, Restitution, PlayerSpeed, kCount };

    // 현재 선택된 파라미터 인덱스.
    int tuningIdx = 0;

    // 튜닝 현재 값 — 초기값은 게임 기본값과 일치.
    float tuningGravity     = 980.0f;   // PhysicsWorld 기본 gravity y.
    float tuningRestitution = 0.0f;     // Dynamic body 기본 반발계수.
    float tuningPlayerSpeed = 1.0f;     // Player 속도 배율 (1.0 = 기본).
#endif

    // ===== 3) ContactCallback 라우팅 ========================================
    //   (a) 플레이어 ground/wall 검출 — Player.EvaluateContact 두 번.
    //   (b) 투사체-플레이어 데미지:
    //        - 양쪽 body 의 userData 를 검사해 Projectile* 찾기.
    //        - 자기 owner 가 쏜 탄은 무시.
    //        - 명중 시 victim.hp -= damage, projectile.expired = true.
    //          (ProjectilePool::Update 가 다음에 정리.)
    ctx.world.SetContactBeginCallback([&](const Contact& c) {
        // (a) 플레이어 contact 처리 — ground / wall 검출.
        p1.EvaluateContact(c);
        p2.EvaluateContact(c);

        // (b) Mortar 폭발 — P8. contact 양쪽 body 의 userData 가 ProjectileKind::Mortar
        //     이면 폭발 트리거 (반경 안 타일 destroy + 6 chunks/타일 burst + radial impulse).
        //     이미 expired 된 mortar 는 무시 (이중 폭발 방지).
        //     본 분기는 일반 데미지 처리 (HandleProjectileContact) 보다 먼저 호출 — mortar
        //     expired=true 마킹이 데미지 분기에 영향을 주지만, mortar 직격 데미지는 없음
        //     (폭발만이 데미지 메커니즘) 이므로 순서 무관.
        auto explodeMortarIfNeeded = [&](BodyId bodyId, Projectile* pj) {
            if (!pj) return;
            if (pj->kind != ProjectileKind::Mortar) return;
            if (pj->expired) return;
            PhysicsBody* b = ctx.world.GetBody(bodyId);
            if (!b) return;
            HandleMortarExplosion(b->pos, ctx.world, *ctx.tiles, *ctx.chunks,
                                  *ctx.slabs);
            // 시각 효과 — 폭발 잔상 (확장 원 + 색/알파 페이드).
            ctx.explosions.push_back(ExplosionEffect{b->pos, 0.0f, 0.4f});
            pj->expired = true;
            audio.PlaySfx("mortar_blast");   // P9 — 폭발 SFX.
        };
        if (PhysicsBody* ba = ctx.world.GetBody(c.a)) {
            explodeMortarIfNeeded(c.a, static_cast<Projectile*>(ba->userData));
        }
        if (PhysicsBody* bb = ctx.world.GetBody(c.b)) {
            explodeMortarIfNeeded(c.b, static_cast<Projectile*>(bb->userData));
        }

        // (c) 투사체 데미지 — P5 Task 5.1 에서 CombatHooks 로 추출.
        //     본 람다는 호출 한 줄. 무기가 3 종 (Rifle/Shotgun/Mortar) 으로
        //     늘어나도 main.cpp 가 두꺼워지지 않게 분리한 결과.
        HandleProjectileContact(c, ctx.projectiles, ctx.world, p1, p2);
    });

    // ===== 4) Fixed-timestep 메인 루프 ======================================
    Timer timer;
    float accumulator = 0.0f;
    int   frameCount  = 0;
    float fpsTimer    = 0.0f;
    bool  running     = true;

    // P10-B Task 7 — 라운드 전환 페이드 알파 (0~200). SmoothStep 이징 적용.
    //   RoundIntro: 서서히 밝아짐 (페이드인 해제), RoundEnd: 서서히 어두워짐.
    float fadeAlpha = 0.0f;

    // P10-B Task 8a — 스코어 바운스 타이머.
    //   RoundEnd 진입 시 kScoreBounceDuration 으로 초기화. OutBounce 이징으로
    //   텍스트 크기 1.5 → 1.0 으로 튕기며 정착.
    float scoreBounceTimer = 0.0f;
    constexpr float kScoreBounceDuration = 0.6f;

    while (running) {
        float ft = timer.Reset();
        if (ft > kMaxFt) ft = kMaxFt;
        accumulator += ft;
        fpsTimer    += ft;
        ++frameCount;

        // ----- SDL 이벤트 (일회성: QUIT / FOCUS_LOST / Title-state KEYDOWN)
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            } else if (e.type == SDL_WINDOWEVENT &&
                       e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                ctx.input.ResetAll();
            } else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                // P4 디버그 토글 (F1~F3) + P7 디버그 토글 (F4/F5) + F9 stress.
                switch (e.key.keysym.scancode) {
                    case SDL_SCANCODE_F1:
                        debugOpts.drawHitboxes = !debugOpts.drawHitboxes;
                        break;
                    case SDL_SCANCODE_F2:
                        debugOpts.drawNormals  = !debugOpts.drawNormals;
                        break;
                    case SDL_SCANCODE_F3:
                        debugOpts.drawImpulses = !debugOpts.drawImpulses;
                        break;
                    case SDL_SCANCODE_F4:
                        // P7 — SpatialGrid (dynamic) cell 라인 토글.
                        debugOpts.drawGrid     = !debugOpts.drawGrid;
                        break;
                    case SDL_SCANCODE_F5:
                        // P7 — 직전 step contact 수 bar 토글.
                        debugOpts.drawPairs    = !debugOpts.drawPairs;
                        break;
                    case SDL_SCANCODE_F6:
                        // P10 — velocity heading 벡터 (파랑 화살표) 토글.
                        debugOpts.drawVelocities = !debugOpts.drawVelocities;
                        break;
                    case SDL_SCANCODE_F7:
                        // P10 — 풀 점유 수 (body / projectile / chunk) TTF 텍스트 토글.
                        debugOpts.drawPoolUsage = !debugOpts.drawPoolUsage;
                        break;
                    case SDL_SCANCODE_F8:
                        // P10 — 런타임 튜닝 슬라이더 토글 (debug build only).
                        debugOpts.drawTuningSliders = !debugOpts.drawTuningSliders;
                        break;
                    case SDL_SCANCODE_F9: {
                        // P7 — 200+ body 부하 시연. 50 개 작은 random circle 을
                        // 화면 상단 (y∈[0,400]) 에 spawn. 좌/우 random vel +
                        // restitution 0.4 로 floor 에 튀어다님.
                        //   - 결정론: F9 자체가 사용자 직접 입력이라 같은 시각에
                        //     같은 시퀀스 누르면 같은 결과. 단 std::rand() 의
                        //     thread-local seed 는 빌드 / 실행 간 같다.
                        //   - 본 분기는 모든 상태에서 동작하되 floor / 천장 등이
                        //     없으면 화면 밖으로 흩어짐.
                        for (int i = 0; i < 50; ++i) {
                            PhysicsBody c;
                            c.pos = { static_cast<float>(std::rand() % kScreenW),
                                      static_cast<float>(std::rand() % 400)        };
                            c.vel = { static_cast<float>(std::rand() % 200 - 100),
                                      0.0f };
                            c.shape       = ShapeType::Circle;
                            c.half        = {6.0f, 6.0f};
                            c.SetMass(0.3f);
                            c.restitution = 0.4f;
                            c.friction    = 0.0f;
                            c.type        = BodyType::Dynamic;
                            ctx.world.CreateBody(c);
                        }
                        break;
                    }
#ifdef CRUMBLE_DEBUG
                    case SDL_SCANCODE_TAB: {
                        // F8 튜닝 활성 중일 때만 Tab → 다음 파라미터로 순환.
                        if (debugOpts.drawTuningSliders) {
                            tuningIdx = (tuningIdx + 1) %
                                static_cast<int>(TuningParam::kCount);
                        }
                        break;
                    }
                    case SDL_SCANCODE_LEFT: {
                        // F8 튜닝 활성 중일 때만 Left → 현재 파라미터 값 감소.
                        if (debugOpts.drawTuningSliders) {
                            if (tuningIdx == static_cast<int>(TuningParam::Gravity)) {
                                tuningGravity = std::max(0.0f, tuningGravity - 50.0f);
                                ctx.world.SetGravity({0.0f, tuningGravity});
                            } else if (tuningIdx == static_cast<int>(TuningParam::Restitution)) {
                                tuningRestitution = std::max(0.0f, tuningRestitution - 0.05f);
                            } else if (tuningIdx == static_cast<int>(TuningParam::PlayerSpeed)) {
                                tuningPlayerSpeed = std::max(0.1f, tuningPlayerSpeed - 0.1f);
                            }
                        }
                        break;
                    }
                    case SDL_SCANCODE_RIGHT: {
                        // F8 튜닝 활성 중일 때만 Right → 현재 파라미터 값 증가.
                        if (debugOpts.drawTuningSliders) {
                            if (tuningIdx == static_cast<int>(TuningParam::Gravity)) {
                                tuningGravity = std::min(3000.0f, tuningGravity + 50.0f);
                                ctx.world.SetGravity({0.0f, tuningGravity});
                            } else if (tuningIdx == static_cast<int>(TuningParam::Restitution)) {
                                tuningRestitution = std::min(1.0f, tuningRestitution + 0.05f);
                            } else if (tuningIdx == static_cast<int>(TuningParam::PlayerSpeed)) {
                                tuningPlayerSpeed = std::min(5.0f, tuningPlayerSpeed + 0.1f);
                            }
                        }
                        break;
                    }
                    case SDL_SCANCODE_F12: {
                        // F12 — 현재 PhysicsWorld 의 모든 body 데이터를 복사한 뒤
                        //       ThreadPool worker 에게 JSON 직렬화 + 파일 쓰기를 위임.
                        //
                        // 결정론 보호:
                        //   - body 데이터를 vector 로 **복사** 한 뒤 Submit.
                        //   - worker 는 복사본만 읽으므로 PhysicsWorld 에 접근 없음
                        //     → 비협상 불변식 (단일 스레드 물리 코어) 유지.
                        //
                        // 출처:
                        //   - 메인 플랜 P10 Task 10 F12 스냅샷 명세.
                        //   - [2. Parallel Programming.pdf p.21-24] 잡 큐 패턴.

                        // body 스냅샷 구조체 — worker thread 가 읽을 POD 복사본.
                        struct BodySnapshot {
                            float posX, posY;       // 위치.
                            float velX, velY;       // 속도.
                            float halfX, halfY;     // 반폭.
                            int   type;             // BodyType (int 변환).
                            int   shape;            // ShapeType (int 변환).
                        };

                        // 살아있는 body 를 모두 복사 (메인 스레드에서 동기 수행).
                        std::vector<BodySnapshot> snap;
                        snap.reserve(static_cast<std::size_t>(
                            ctx.world.BodyCapacity()));
                        for (int bi = 0; bi < ctx.world.BodyCapacity(); ++bi) {
                            const PhysicsBody* b = ctx.world.GetBody(bi);
                            if (!b) continue;
                            BodySnapshot s;
                            s.posX  = b->pos.x;  s.posY  = b->pos.y;
                            s.velX  = b->vel.x;  s.velY  = b->vel.y;
                            s.halfX = b->half.x; s.halfY = b->half.y;
                            s.type  = static_cast<int>(b->type);
                            s.shape = static_cast<int>(b->shape);
                            snap.push_back(s);
                        }

                        // 복사본을 람다로 캡처해 worker thread 에 위임.
                        threadPool.Submit([snap = std::move(snap)]() {
                            // nlohmann::json 으로 배열 직렬화.
                            nlohmann::json arr = nlohmann::json::array();
                            for (const auto& s : snap) {
                                arr.push_back({
                                    {"pos",   {s.posX,  s.posY}},
                                    {"vel",   {s.velX,  s.velY}},
                                    {"half",  {s.halfX, s.halfY}},
                                    {"type",  s.type},
                                    {"shape", s.shape}
                                });
                            }
                            // snapshot.json 파일로 덤프 (게임 실행 디렉터리).
                            std::ofstream ofs("snapshot.json");
                            if (ofs) {
                                ofs << arr.dump(2);
                            } else {
                                std::fprintf(stderr,
                                    "[F12] snapshot.json write failed\n");
                            }
                        });
                        break;
                    }
#endif  // CRUMBLE_DEBUG
                    default:
                        break;
                }
                // Title 화면에서 E 키 — MapEditor 진입.
                // RunMapEditor 는 자체 SDL 루프를 실행하고 ESC 시 반환.
                // 반환 후 이 메인 루프가 Title 에서 그대로 재개된다.
                if (e.key.keysym.scancode == SDL_SCANCODE_E &&
                    ctx.flow.State() == FlowState::Title) {
                    // fontHud 는 렌더 블록 지역 변수라 여기서는 GetFont 로 직접 얻는다.
                    RunMapEditor(window, renderer,
                                 resources.GetFont("hud", 22));
                }

                // Title / Controls / MatchEnd 에서만 OnAnyKey — Playing 중의
                // 키는 InputManager.PumpFromSDL 가 처리.
                // P9 — Title 에서 자산 로드 미완료면 진행 차단 (Loading... 표시).
                FlowState s = ctx.flow.State();
                const bool blockTitle = (s == FlowState::Title) &&
                    !assetsReady.load(std::memory_order_acquire);
                if (!blockTitle && (s == FlowState::Title ||
                                    s == FlowState::Controls ||
                                    s == FlowState::MatchEnd)) {
                    bool wasMatchEnd = (s == FlowState::MatchEnd);
                    ctx.flow.OnAnyKey();
                    // MatchEnd → Title 전이라면 점수 / 라운드 리셋 호출.
                    if (wasMatchEnd && ctx.flow.State() == FlowState::Title) {
                        ctx.flow.RestartMatch();
                    }
                    // Controls → RoundIntro 전이라면 라운드 시작 setup.
                    if (ctx.flow.State() == FlowState::RoundIntro) {
                        StartRound(ctx, mapPool);
                        audio.PlayBgm("bgm_battle");      // P9 — 매치 BGM.
                        audio.PlaySfx("round");           // 라운드 시작 컷.
                    }
                    // Title 복귀 시 — bgm_title 재생.
                    if (wasMatchEnd && ctx.flow.State() == FlowState::Title) {
                        audio.PlayBgm("bgm_title");
                    }
                }
            }
        }

        // ----- Fixed-step update -----
        // P9 — 자산 로드 완료 직후 한 번만 bgm_title 재생. Title state 가 끝나
        //   도 BGM 은 RoundIntro 진입 시 bgm_battle 로 자연 교체된다.
        if (!titleBgmStarted && assetsReady.load(std::memory_order_acquire)) {
            audio.PlayBgm("bgm_title");
            titleBgmStarted = true;
        }

        while (accumulator >= kFixedDt) {
            ctx.input.BeginFrame();
            ctx.input.PumpFromSDL();

            // GameFlow 의 자동 전이 (RoundIntro 3s, RoundEnd 3s, MatchEnd 5s).
            FlowState prevState = ctx.flow.State();
            ctx.flow.Update(kFixedDt);
            FlowState curState  = ctx.flow.State();

            // RoundEnd → RoundIntro 전이 시 라운드 setup + BGM / round SFX.
            if (prevState == FlowState::RoundEnd &&
                curState  == FlowState::RoundIntro) {
                StartRound(ctx, mapPool);
                audio.PlaySfx("round");           // P9 — 라운드 시작 컷.
                // bgm_battle 은 첫 라운드부터 재생 중이라 재호출 불필요.
            }

            // P10-B Task 8a — RoundEnd 진입 시 scoreBounceTimer 시작.
            //   prevState 가 RoundEnd 가 아닌데 curState 가 RoundEnd 이면 첫 진입.
            if (prevState != FlowState::RoundEnd &&
                curState  == FlowState::RoundEnd) {
                scoreBounceTimer = kScoreBounceDuration;
            }
            // scoreBounceTimer 감산 (0 미만 클램프).
            if (scoreBounceTimer > 0.0f) {
                scoreBounceTimer -= kFixedDt;
                if (scoreBounceTimer < 0.0f) scoreBounceTimer = 0.0f;
            }

            // P10-B Task 7 — 라운드 전환 페이드 알파 갱신.
            //   RoundIntro: SmoothStep(t/3) 역방향 — 시작은 검게, 끝은 투명.
            //   RoundEnd  : SmoothStep(t/3) 순방향 — 시작은 투명, 끝은 검게.
            //   기타 상태  : 알파 0 (페이드 없음).
            {
                const float st = ctx.flow.StateTimer();
                if (curState == FlowState::RoundIntro) {
                    // t=0 → alpha=200 (어둠), t=3 → alpha=0 (밝음).
                    fadeAlpha = 200.0f * (1.0f - Ease::SmoothStep(st / 3.0f));
                } else if (curState == FlowState::RoundEnd) {
                    // t=0 → alpha=0 (밝음), t=3 → alpha=200 (어둠).
                    fadeAlpha = 200.0f * Ease::SmoothStep(st / 3.0f);
                } else {
                    fadeAlpha = 0.0f;
                }
            }

            if (ctx.flow.IsCombatActive()) {
                // -- 입력 → 이동 / 점프 / (MortarAiming 시 게이지/각도) --
                //   facingSign_ 은 Player::UpdateInput 안에서 자동 갱신.
                p1.UpdateInput(kFixedDt);
                p2.UpdateInput(kFixedDt);

                // -- 무기 전환 + 발사 분기 (P5) --
                //   본 람다는 무기 슬롯 키 (1/2/3) 와 발사 키를 같이 처리.
                //   각 무기 종류별로:
                //     - Rifle   : facing 방향 직사.
                //     - Shotgun : facing 방향 + recoil 임펄스 (슈터 뒤로 밀림).
                //     - Mortar  : 발사 키 press 시 EnterMortarAiming —
                //                 실제 발사는 release 에서 Player::UpdateInput
                //                 의 MortarAiming 분기가 처리.
                auto handlePlayerFire = [&](Player& p, PlayerWeapons& pw,
                                            const std::string& prefix) {
                    // 무기 전환 — currentIdx 변경 전 현재 무기의 OnSwitchedOut
                    // 호출 (Mortar 의 aim 위상 리셋 등).
                    if (ctx.input.IsPressed(prefix + "_weapon_1")) {
                        pw.Current()->OnSwitchedOut();
                        pw.currentIdx = 0;
                        audio.PlaySfx("swap");          // P9 — 무기 교체 SFX.
                    }
                    if (ctx.input.IsPressed(prefix + "_weapon_2")) {
                        pw.Current()->OnSwitchedOut();
                        pw.currentIdx = 1;
                        audio.PlaySfx("swap");
                    }
                    if (ctx.input.IsPressed(prefix + "_weapon_3")) {
                        pw.Current()->OnSwitchedOut();
                        pw.currentIdx = 2;
                        audio.PlaySfx("swap");
                    }

                    // 발사 키 처리 — Mortar 진입 / Rifle/Shotgun 자동 연사.
                    //   - Rifle / Shotgun: IsHeld (hold 중 매 frame 시도 — 무기
                    //     자체의 reload/ammo 가드가 자연스러운 fire-rate 한계).
                    //   - Mortar       : IsPressed (rising edge — 진입은 한 번
                    //     만, hold 는 Player::UpdateInput 가 게이지 진동에 사용).
                    //   - IsMortarAiming 가드: 이미 조준 중이면 본 분기 무시
                    //     (조준 중에는 Player 가 입력을 단독 소화).
                    const bool firePressed = ctx.input.IsPressed(prefix + "_fire");
                    const bool fireHeld    = ctx.input.IsHeld   (prefix + "_fire");
                    if (!p.IsMortarAiming()) {
                        auto* b = p.GetBody();
                        if (!b) return;
                        // 발사 방향 — facing 기준 horizontal + 살짝 위쪽 기울기.
                        //   - 순수 (±1, 0) 직사는 중력 g=980 + lifetime 1.5s 환경
                        //     에서 1100+ px 까지 빨리 꽂혀 시각적으로 "땅으로
                        //     박는 듯한" 인상 (사용자 P7 시각 검증 단계 피드백).
                        //   - y = -kFireSlope (위쪽) 로 약 5.7° 기울임 → 총알이
                        //     자연스러운 포물선 호.
                        //   - Rifle / Shotgun 의 base 방향에만 적용. Mortar 는 자체
                        //     dual-oscillation aim 사용이라 본 분기 무관.
                        constexpr float kFireSlope = 0.10f;
                        Vec2 dir{static_cast<float>(p.FacingSign()), -kFireSlope};
                        // 총구 위치 — body 중심에서 facing 방향으로 30px 전진 +
                        // 살짝 위. 스프라이트 무기의 총구 끝과 시각적으로 일치
                        // 시키고, body 밖에서 spawn 되어 슈터 자기 자신과의
                        // 초기 overlap (P3 결함 #12 의 false wall detection) 도
                        // 회피한다. (Mortar 는 자체 aim-기반 spawn 사용.)
                        const Vec2 muzzle{
                            b->pos.x + static_cast<float>(p.FacingSign()) * 30.0f,
                            b->pos.y - 2.0f};

                        if (pw.currentIdx == 2) {
                            // Mortar — press 시 한 번만 진입 (hold 자동연사 X).
                            //   추가 가드: Grounded / WallSlide 상태에서만 조준 모드
                            //   진입 가능. 공중 (Airborne) 에서는 박격포 사용 불가
                            //   — Kinematic 동결로 공중 정지하는 시각이 부자연스러움.
                            //   사용자 요청 (P6 시각 검증 단계).
                            const PlayerState ps = p.GetState();
                            const bool canAimMortar =
                                (ps == PlayerState::Grounded) ||
                                (ps == PlayerState::WallSlide);
                            if (firePressed && !pw.mortar->IsReloading() && canAimMortar) {
                                p.EnterMortarAiming();
                                audio.PlaySfx("select");     // P9 — 박격포 조준 진입.
                            }
                        } else if (pw.currentIdx == 0) {
                            // Rifle — hold 자동 연사. ammo / reload 가 cooldown.
                            if (fireHeld) {
                                if (pw.rifle->TryFire(muzzle, dir)) {
                                    audio.PlaySfx("rifle");  // P9 — 발사 성공 시.
                                }
                            }
                        } else if (pw.currentIdx == 1) {
                            // Shotgun — hold 자동 연사. 발사 성공 시 recoil
                            // 즉시 ApplyImpulse (Differentiated knockback).
                            if (fireHeld) {
                                if (pw.shotgun->TryFire(muzzle, dir)) {
                                    b->ApplyImpulse(pw.shotgun->LastRecoilImpulse());
                                    audio.PlaySfx("shotgun");
                                }
                            }
                        }
                    }

                    // 모든 무기의 reload 타이머 갱신. 활성 무기와 무관하게
                    // 들고있지 않은 무기도 자체적으로 재장전이 진행되어
                    // 전환 직후 즉시 사용 가능.
                    pw.rifle  ->Update(kFixedDt);
                    pw.shotgun->Update(kFixedDt);
                    pw.mortar ->Update(kFixedDt);
                };
                handlePlayerFire(p1, ctx.pw1, "p1");
                handlePlayerFire(p2, ctx.pw2, "p2");

                // -- P6 MovingPlatform 갱신 --
                //   world.Step 직전에 호출해 본 step 의 vel/pos 가 충돌 응답에
                //   반영되도록. Kinematic body 라 적분기는 본 vel/forceAccum 을
                //   무시하고, 충돌 응답에서만 상대 속도를 추정하는 데 사용.
                for (auto& pf : ctx.platforms) {
                    pf->Update(kFixedDt);
                }

                // -- 물리 / 투사체 / FSM 갱신 --
                //   P9 — HP 델타 검출용으로 step 직전 값 캡처. ContactCallback
                //   안에서 직접 audio 호출은 callback 의 람다 캡처 의존성을
                //   늘리므로 본 위치에서 step 후 비교로 처리.
                const int p1HpBefore = p1.Stats().hp;
                const int p2HpBefore = p2.Stats().hp;
                ctx.world.Step(kFixedDt);            // ContactCallback 실행.
                if (p1.Stats().hp < p1HpBefore) {
                    audio.PlaySfx("hit");            // P9 — 명중 SFX.
                    // P10-B Task 6 — P1 피격 플래시 0.15 초 시작.
                    p1.Stats().hitFlashTimer = 0.15f;
                }
                if (p2.Stats().hp < p2HpBefore) {
                    audio.PlaySfx("hit");            // P9 — 명중 SFX.
                    // P10-B Task 6 — P2 피격 플래시 0.15 초 시작.
                    p2.Stats().hitFlashTimer = 0.15f;
                }
                ctx.projectiles.Update(kFixedDt);    // expired/lifetime 정리.
                if (ctx.chunks) ctx.chunks->Update(kFixedDt);  // P8 chunks lifetime / offscreen.

                // P8 폭발 시각 효과 — lifetime 진행 + 만료 제거 (swap-and-pop).
                for (std::size_t i = 0; i < ctx.explosions.size(); ) {
                    ctx.explosions[i].lifetime += kFixedDt;
                    if (ctx.explosions[i].lifetime >= ctx.explosions[i].maxLifetime) {
                        ctx.explosions[i] = ctx.explosions.back();
                        ctx.explosions.pop_back();
                    } else {
                        ++i;
                    }
                }
                p1.UpdateFSM(kFixedDt);
                p2.UpdateFSM(kFixedDt);

                // P10-B Task 6 — 피격 플래시 타이머 감산 (0 미만으로 가지 않도록).
                if (p1.Stats().hitFlashTimer > 0.0f)
                    p1.Stats().hitFlashTimer -= kFixedDt;
                if (p2.Stats().hitFlashTimer > 0.0f)
                    p2.Stats().hitFlashTimer -= kFixedDt;

                // -- 추락 데미지 + 리스폰 --
                //   floor 두께가 얇아 빠른 속도에선 통과 가능. 화면 아래로 일정
                //   이상 떨어지면 HP 절반 + 스폰 위치로 복귀 (메인 스펙 §3.4).
                auto checkFall = [&](Player& p) {
                    if (auto* b = p.GetBody()) {
                        if (b->pos.y > kFallY) {
                            p.Stats().hp = std::max(1, p.Stats().hp / 2);
                            p.Respawn();
                        }
                    }
                };
                checkFall(p1);
                checkFall(p2);

                // -- 라운드 종료 검사 --
                //   HP 가 정확히 0 인 시점에 한 번 트리거. OnRoundOver 가
                //   Playing 외에서는 무시되므로 이중 호출 안전.
                if (p1.Stats().hp <= 0) {
                    p1.Stats().alive = false;
                    ctx.flow.OnRoundOver(1);   // P2 승.
                } else if (p2.Stats().hp <= 0) {
                    p2.Stats().alive = false;
                    ctx.flow.OnRoundOver(0);   // P1 승.
                }
            }

            accumulator -= kFixedDt;
        }

        // ===== 렌더링 =======================================================
        SDL_SetRenderDrawColor(renderer, 30, 30, 50, 255);
        SDL_RenderClear(renderer);

        // 1) 정적 타일.
        for (BodyId id : ctx.staticTileIds) {
            if (auto* b = ctx.world.GetBody(id)) {
                DrawAabb(renderer, *b, 90, 90, 90);
            }
        }

        // 1.5) MovingPlatform (P6) — Kinematic body 색 (청록) 으로 구분.
        //     P4 디버그 오버레이의 Kinematic 색 (파랑 외곽선) 과 시각 호응.
        for (const auto& pf : ctx.platforms) {
            if (auto* b = ctx.world.GetBody(pf->GetBodyId())) {
                DrawAabb(renderer, *b, 60, 200, 200);
            }
        }

        // 1.7) Destructible TileGrid (P8) — alive 타일을 "하나의 솔리드 벽" 으로
        //     그린다. 파괴 불가능한 회색 정적 벽 (90,90,90) 과 색상만 다르게
        //     (따뜻한 벽돌/벽 톤) 표시. 타일 경계선(외곽선)을 그리지 않으므로
        //     16px 정수 그리드 위의 인접 타일들이 경계 없이 정확히 맞붙어
        //     조각조각이 아니라 한 벽처럼 보인다 (사용자 요청). 파괴되면 그 타일
        //     만 사라져 벽에 구멍이 뚫린 모습.
        //     [정확 매핑 / 투명벽 방지] 그리는 rect 는 물리 타일 body 와 동일한
        //     중심·크기 (kTileSize) — 시각과 충돌이 1:1 이라 "보이지 않는 벽"
        //     이나 "충돌 없는 그림" 이 생기지 않는다. 확장/축소 금지.
        if (ctx.tiles) {
            const auto& alive   = ctx.tiles->AliveFlags();
            const auto& centers = ctx.tiles->Centers();
            SDL_SetRenderDrawColor(renderer, 175, 140, 105, 255);
            for (std::size_t i = 0; i < alive.size(); ++i) {
                if (!alive[i]) continue;
                SDL_Rect r = {
                    static_cast<int>(centers[i].x - TileGrid::kTileSize * 0.5f),
                    static_cast<int>(centers[i].y - TileGrid::kTileSize * 0.5f),
                    static_cast<int>(TileGrid::kTileSize),
                    static_cast<int>(TileGrid::kTileSize),
                };
                SDL_RenderFillRect(renderer, &r);
            }
        }

        // 1.75) Breakable slabs — 한 솔리드 사각형으로 그림. 물리 Static body
        //     의 AABB 와 정확히 일치 (투명벽/phantom 없음). 파괴되면 body·렌더
        //     동시 제거. 파괴 불가능한 회색 정적 벽과 색상으로 구분.
        if (ctx.slabs) ctx.slabs->Render(renderer);

        // 1.8) Chunks (P8) — 작은 어두운 박스 (잔해 시각). dynamic body 라
        //     중력 + 충돌 응답으로 자연스럽게 떨어진다.
        if (ctx.chunks) {
            ctx.chunks->Storage().ForEachAlive([&](int /*id*/, Chunk& k) {
                if (auto* b = ctx.world.GetBody(k.bodyId)) {
                    DrawAabb(renderer, *b, 90, 70, 50);
                }
            });
        }

        // 1.9) ExplosionEffect (P10-B Task 8b 개선) — LerpColor 3단계 그라디언트.
        //     - radius  : OutQuad 이징으로 빠르게 확장 후 감속 (36 → 84 px).
        //     - color   : t<0.3 흰→노랑, t<0.7 노랑→주황, t>=0.7 주황→어두운빨강.
        //     - alpha   : (1-t) 선형 페이드아웃.
        //     - 두 동심원: 바깥(짙음/큰반경) + 안쪽(밝음/절반반경).
        //     [8. Numerical Analysis 1.pdf p.2] LerpColor 사용.
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        for (const auto& e : ctx.explosions) {
            const float t = e.lifetime / e.maxLifetime;    // 0 ~ 1.

            // OutQuad 이징 — 반경이 빠르게 확장 후 감속.
            const float eased  = Ease::OutQuad(t);         // 0 ~ 1 감속 곡선.
            const float radius = 36.0f + eased * 48.0f;   // 36 ~ 84 px.

            // 3단계 색상 그라디언트 (LerpColor 사용).
            const SDL_Color kWhite   = {255, 255, 220, 255};
            const SDL_Color kYellow  = {255, 220,  30, 255};
            const SDL_Color kOrange  = {220,  80,  20, 255};
            const SDL_Color kDarkRed = {100,  10,   5, 255};
            SDL_Color baseColor;
            if (t < 0.3f) {
                // 0~0.3: 흰색 → 노랑.
                baseColor = LerpColor(kWhite,  kYellow, t / 0.3f);
            } else if (t < 0.7f) {
                // 0.3~0.7: 노랑 → 주황.
                baseColor = LerpColor(kYellow, kOrange, (t - 0.3f) / 0.4f);
            } else {
                // 0.7~1.0: 주황 → 어두운빨강.
                baseColor = LerpColor(kOrange, kDarkRed, (t - 0.7f) / 0.3f);
            }

            // 알파 페이드아웃 — t=0 최대 알파, t=1 완전 투명.
            const Uint8 outerA = static_cast<Uint8>(180.0f * (1.0f - t));
            const Uint8 innerA = static_cast<Uint8>(230.0f * (1.0f - t));

            // 바깥 원 — 기본 색에 낮은 알파 (충격파 링).
            SDL_SetRenderDrawColor(renderer,
                                   baseColor.r, baseColor.g, baseColor.b, outerA);
            DrawFilledCircle(renderer,
                             static_cast<int>(e.center.x),
                             static_cast<int>(e.center.y),
                             static_cast<int>(radius));

            // 안쪽 원 — 더 밝은 코어 (t 초반일수록 흰빛). 반경 절반.
            const SDL_Color innerColor = LerpColor(kWhite, baseColor, t);
            SDL_SetRenderDrawColor(renderer,
                                   innerColor.r, innerColor.g, innerColor.b, innerA);
            DrawFilledCircle(renderer,
                             static_cast<int>(e.center.x),
                             static_cast<int>(e.center.y),
                             static_cast<int>(radius * 0.5f));
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        // 2) 두 플레이어 — PNG 스프라이트 (차콜 캡슐 몸체 + 팀색 바이저/백팩)
        //    + 현재 든 무기 스프라이트. 텍스처 부재 시 기존 단색 AABB fallback.
        //    애니메이션 프레임: Airborne/WallSlide=jump(5), |vel.x|>20=run(1~4),
        //    그 외 idle(0). facingSign<0 이면 수평 flip.
        auto drawPlayerSprite = [&](Player& pl, PlayerWeapons& pw,
                                    int teamIdx, float& anim) {
            const PhysicsBody* b = pl.GetBody();
            if (!b) return;

            SDL_Texture* sheet = texPlayer[teamIdx];
            if (!sheet) {
                // fallback — 기존 단색 박스 (P10-B 피격 흰색 플래시 포함).
                Uint8 R = (teamIdx == 0) ? 220 : 30;
                Uint8 G = (teamIdx == 0) ?  30 : 80;
                Uint8 B = (teamIdx == 0) ?  30 : 220;
                if (pl.Stats().hitFlashTimer > 0.0f) { R = G = B = 255; }
                DrawAabb(renderer, *b, R, G, B);
                return;
            }

            constexpr int kFrameW = 64, kFrameH = 64;
            const PlayerState st = pl.GetState();
            int frame;
            if (st == PlayerState::Airborne || st == PlayerState::WallSlide) {
                frame = 5;                                  // jump (다리 모음).
                anim  = 0.0f;
            } else if (std::fabs(b->vel.x) > 20.0f) {
                anim += 1.0f / 60.0f;                       // run-cycle 진행.
                frame = 1 + (static_cast<int>(anim / 0.1f) % 4);
            } else {
                anim  = 0.0f;
                frame = 0;                                  // idle.
            }

            // 스프라이트 feet(62)·가로중심(32) 을 물리 body 에 정렬.
            SDL_Rect src = {frame * kFrameW, 0, kFrameW, kFrameH};
            SDL_Rect dst = {
                static_cast<int>(b->pos.x - 32.0f),
                static_cast<int>(b->pos.y + b->half.y - 62.0f),
                kFrameW, kFrameH,
            };
            const SDL_RendererFlip flip =
                (pl.FacingSign() < 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
            SDL_RenderCopyEx(renderer, sheet, &src, &dst, 0.0, nullptr, flip);

            // 든 무기 (박격포 조준 중에는 aim 시각화가 대신하므로 생략).
            if (!pl.IsMortarAiming()) {
                SDL_Texture* wtex = texWeapon[pw.currentIdx];
                if (wtex) {
                    int ww = 0, wh = 0;
                    SDL_QueryTexture(wtex, nullptr, nullptr, &ww, &wh);
                    const int   facing = pl.FacingSign();
                    const int   gx = kWeaponGripX[pw.currentIdx];
                    const int   gy = kWeaponGripY[pw.currentIdx];
                    // 손 위치 ≈ body 중심 살짝 앞/위. grip 픽셀을 손에 정렬.
                    const float handX = b->pos.x + facing * 2.0f;
                    const float handY = b->pos.y - 2.0f;
                    SDL_Rect wdst;
                    wdst.w = ww; wdst.h = wh;
                    wdst.x = (facing >= 0)
                             ? static_cast<int>(handX - gx)
                             : static_cast<int>(handX - (ww - gx));
                    wdst.y = static_cast<int>(handY - gy);
                    const SDL_RendererFlip wflip =
                        (facing < 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
                    SDL_RenderCopyEx(renderer, wtex, nullptr, &wdst, 0.0,
                                     nullptr, wflip);
                }
            }

            // 피격 플래시 — 반투명 흰색 오버레이 (P10-B Task 6).
            if (pl.Stats().hitFlashTimer > 0.0f) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 140);
                SDL_Rect fr = {
                    static_cast<int>(b->pos.x - b->half.x),
                    static_cast<int>(b->pos.y - b->half.y),
                    static_cast<int>(b->half.x * 2.0f),
                    static_cast<int>(b->half.y * 2.0f),
                };
                SDL_RenderFillRect(renderer, &fr);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }
        };
        drawPlayerSprite(p1, ctx.pw1, 0, spriteAnim[0]);
        drawPlayerSprite(p2, ctx.pw2, 1, spriteAnim[1]);

        // 3) 투사체 (살아있는 것만). owner 색의 작은 사각형 (P3 결함 fix #7
        //    AABB 표현). P1 = 노랑 톤, P2 = 보라 톤 — 두 측 색을 시각적으로
        //    구분하기 위함.
        ctx.projectiles.Storage().ForEachAlive([&](int /*id*/, Projectile& pj) {
            if (auto* b = ctx.world.GetBody(pj.bodyId)) {
                Uint8 R = (pj.ownerPlayerIndex == 0) ? 240 : 120;
                Uint8 G = (pj.ownerPlayerIndex == 0) ? 200 : 200;
                Uint8 B = (pj.ownerPlayerIndex == 0) ? 120 : 240;
                DrawAabb(renderer, *b, R, G, B);
            }
        });

        // 4) Mortar 조준 시각화 (P5) — IsMortarAiming 인 플레이어 위에
        //    방향선 (60 px) + gauge bar (수직 30 px). gauge 의 fill 비율은
        //    speed 의 min~max 범위에서 0~1 로 매핑.
        auto drawMortarAim = [&](Player& p, PlayerWeapons& pw) {
            if (!p.IsMortarAiming() || pw.mortar == nullptr) return;
            const PhysicsBody* b = p.GetBody();
            if (!b) return;
            const MortarAimState& aim = pw.mortar->Aim();

            // 방향선 — facing 방향으로 60 px 길이.
            Vec2 d = aim.CurrentDirection();
            SDL_SetRenderDrawColor(renderer, 240, 200, 60, 255);
            SDL_RenderDrawLine(renderer,
                static_cast<int>(b->pos.x),
                static_cast<int>(b->pos.y),
                static_cast<int>(b->pos.x + d.x * 60.0f),
                static_cast<int>(b->pos.y + d.y * 60.0f));

            // Gauge bar — 머리 위 수직 막대. min 일 때 비어있고 max 면 가득.
            float t = (aim.CurrentSpeed() - MortarAimState::kMinSpeed) /
                      (MortarAimState::kMaxSpeed - MortarAimState::kMinSpeed);
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            int gaugeX = static_cast<int>(b->pos.x) - 2;
            int gaugeY = static_cast<int>(b->pos.y - b->half.y - 40.0f);
            SDL_Rect gaugeBg = {gaugeX, gaugeY, 4, 30};
            // fill 은 아래에서 위로 차오름 — y 좌표를 (1 - t) 만큼 내린 후
            // 높이를 t * 30 으로.
            SDL_Rect gaugeFill = {
                gaugeX,
                gaugeY + static_cast<int>(30.0f * (1.0f - t)),
                4,
                static_cast<int>(30.0f * t)
            };
            SDL_SetRenderDrawColor(renderer,  60,  60,  60, 255);
            SDL_RenderFillRect(renderer, &gaugeBg);
            SDL_SetRenderDrawColor(renderer, 240,  80,  80, 255);
            SDL_RenderFillRect(renderer, &gaugeFill);
        };
        drawMortarAim(p1, ctx.pw1);
        drawMortarAim(p2, ctx.pw2);

        // 5) P4 디버그 오버레이 — F1/F2/F3 토글이 켜진 항목만. HUD 보다 먼저
        //    그려 HP 바 위로 화살표가 가려지지 않도록.
        DrawPhysicsDebug(renderer, ctx.world, debugOpts);

        // 5) HUD — HP 바 + 라운드 dot. 매 frame 항상 그림.
        DrawHUD(renderer, kScreenW, p1, p2,
                ctx.flow.P1Score(), ctx.flow.P2Score());

        // 5) 상태별 UI 텍스트 (P9 Task 9.4) — TTF 가 도착 (assetsReady) 후
        //    Title / Controls / RoundIntro / RoundEnd / MatchEnd 각 상태가 자기
        //    안내 / 결과 텍스트를 그림. 폰트 캐시 miss 면 DrawText 가 no-op.
        TTF_Font* fontTitle   = resources.GetFont("title",   64);
        TTF_Font* fontRegular = resources.GetFont("regular", 28);
        TTF_Font* fontHud     = resources.GetFont("hud",     22);
        const FlowState fs = ctx.flow.State();
        const bool ready = assetsReady.load(std::memory_order_acquire);

        // 5.5) 무기 표시 HUD (사용자 요청) — 각 플레이어 화면 모서리에 반투명
        //      패널 + 현재 무기 스프라이트 아이콘 + 무기 이름을 상시 표시해
        //      "지금 무슨 무기인지" 즉시 확인 가능. 무기 교체 시 pw.currentIdx
        //      가 바뀌므로 다음 frame 에 자동 갱신. P1 = 좌상단, P2 = 우상단,
        //      팀색 띠로 소속 구분. 전투 활성(IsCombatActive) 중에만 표시.
        if (ctx.flow.IsCombatActive()) {
            auto drawWeaponBadge = [&](bool leftSide, PlayerWeapons& pw, int teamIdx) {
                static const char* const kNames[3] = {"RIFLE", "SHOTGUN", "MORTAR"};
                const int idx = pw.currentIdx;
                const int bw = 170, bh = 34, margin = 12, top = 56;
                const int px = leftSide ? margin : (kScreenW - bw - margin);
                const Uint8 tr = (teamIdx == 0) ? 220 : 40;
                const Uint8 tg = (teamIdx == 0) ?  40 : 90;
                const Uint8 tb = (teamIdx == 0) ?  40 : 220;

                // 반투명 어두운 패널 + 팀색 좌측 띠.
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 18, 18, 26, 140);
                SDL_Rect panel = {px, top, bw, bh};
                SDL_RenderFillRect(renderer, &panel);
                SDL_SetRenderDrawColor(renderer, tr, tg, tb, 230);
                SDL_Rect strip = {px, top, 4, bh};
                SDL_RenderFillRect(renderer, &strip);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

                // 무기 스프라이트 아이콘 (패널 높이에 맞춰 축소).
                if (SDL_Texture* wtex = texWeapon[idx]) {
                    int iw = 0, ih = 0;
                    SDL_QueryTexture(wtex, nullptr, nullptr, &iw, &ih);
                    float sc = 22.0f / static_cast<float>(ih);
                    if (iw * sc > 52.0f) sc = 52.0f / static_cast<float>(iw);
                    SDL_Rect dst = {
                        px + 12,
                        top + (bh - static_cast<int>(ih * sc)) / 2,
                        static_cast<int>(iw * sc),
                        static_cast<int>(ih * sc),
                    };
                    SDL_RenderCopy(renderer, wtex, nullptr, &dst);
                }

                // 무기 이름 텍스트 (팀색).
                if (fontHud) {
                    DrawText(renderer, fontHud, kNames[idx],
                             px + 72, top + 7,
                             SDL_Color{tr, tg, tb, 255}, false);
                }
            };
            drawWeaponBadge(true,  ctx.pw1, 0);
            drawWeaponBadge(false, ctx.pw2, 1);
        }

        if (fs == FlowState::Title) {
            DrawText(renderer, fontTitle, "CRUMBLE",
                     kScreenW / 2, 240, SDL_Color{220, 220, 220, 255}, true);
            DrawText(renderer, fontRegular,
                     ready ? "Press any key to begin" : "Loading...",
                     kScreenW / 2, 420, SDL_Color{220, 220, 200, 255}, true);
        } else if (fs == FlowState::Controls) {
            DrawText(renderer, fontTitle, "Controls",
                     kScreenW / 2, 80, SDL_Color{220, 220, 220, 255}, true);
            DrawText(renderer, fontRegular, "Player 1 (RED)",
                     320, 180, SDL_Color{220, 30, 30, 255}, true);
            DrawText(renderer, fontRegular, "Player 2 (BLUE)",
                     960, 180, SDL_Color{30, 80, 220, 255}, true);
            const char* p1Keys[] = {
                "Move:    A / D",         "Jump:    W",            "Down:    S",
                "Weapon:  1 / 2 / 3",     "Fire:    F",            "Mortar:  G",
            };
            // 키 라벨은 ASCII 로만 — 게임 폰트(Ubuntu)에 화살표 글리프(U+2190~93)
            // 가 없어 ←↑↓→ 를 쓰면 .notdef(빈 네모)로 렌더돼 "잘려 보임".
            // 방향키는 "Left/Right/Up/Down Arrow" 로 명시한다.
            const char* p2Keys[] = {
                "Move:    Left / Right Arrow",
                "Jump:    Up Arrow",
                "Down:    Down Arrow",
                "Weapon:  Numpad 1 / 2 / 3",
                "Fire:    Numpad 0",
                "Mortar:  Numpad Enter",
            };
            for (int i = 0; i < 6; ++i) {
                DrawText(renderer, fontRegular, p1Keys[i],
                         180, 240 + i * 40, SDL_Color{220, 220, 220, 255}, false);
                DrawText(renderer, fontRegular, p2Keys[i],
                         820, 240 + i * 40, SDL_Color{220, 220, 220, 255}, false);
            }
            DrawText(renderer, fontRegular, "Press any key to start",
                     kScreenW / 2, 600, SDL_Color{220, 220, 200, 255}, true);
        } else if (fs == FlowState::RoundIntro) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "Round %d", ctx.flow.Round());
            DrawText(renderer, fontTitle, buf,
                     kScreenW / 2, kScreenH / 2,
                     SDL_Color{255, 220, 60, 255}, true);
        } else if (fs == FlowState::RoundEnd) {
            const int w = ctx.flow.RoundWinnerIdx();
            const SDL_Color col = (w == 0) ? SDL_Color{220, 30, 30, 255}
                                            : SDL_Color{30, 80, 220, 255};
            // P10-B Task 8a — OutBounce pop-and-settle 스케일 애니메이션.
            //   bounceT: 0 (시작) → 1 (끝).
            //   전반 (t<0.3): 1.0 → 1.5 확대 (pop). 후반 (t>=0.3): 1.5 → 1.0 바운스 (settle).
            //   스펙 §3.4 — "scale 1.0 → 1.5 → 1.0" pop-and-settle 패턴.
            {
                const float bounceT = 1.0f - (scoreBounceTimer / kScoreBounceDuration);
                float scale;
                if (bounceT < 0.3f) {
                    scale = Lerp(1.0f, 1.5f, bounceT / 0.3f);
                } else {
                    scale = Lerp(1.5f, 1.0f, Ease::OutBounce((bounceT - 0.3f) / 0.7f));
                }
                // 현재 배율을 먼저 읽는다 — SDL_RenderSetLogicalSize 가 설정한
                // "논리 맞춤 배율"이 여기에 들어 있다(창 리사이즈 시 SDL 이 자동 갱신).
                // 바운스는 이 배율 위에 곱해야 하고, 복구도 1.0 이 아니라 이 값으로
                // 되돌려야 한다. (1.0 으로 복구하면 논리 배율이 날아가 라운드 종료 후
                // 화면이 1:1 로 작아진다.)
                float baseSx, baseSy;
                SDL_RenderGetScale(renderer, &baseSx, &baseSy);
                SDL_RenderSetScale(renderer, baseSx * scale, baseSy * scale);
                // 스케일된 좌표계에서 중앙 계산 — 논리 좌표를 bounce(scale) 로 나눔.
                DrawText(renderer, fontTitle,
                         w == 0 ? "P1 Wins Round" : "P2 Wins Round",
                         static_cast<int>(kScreenW / 2 / scale),
                         static_cast<int>(kScreenH / 2 / scale),
                         col, true);
                SDL_RenderSetScale(renderer, baseSx, baseSy);  // 논리 배율로 복구.
            }
        } else if (fs == FlowState::MatchEnd) {
            const int w = ctx.flow.MatchWinnerIdx();
            const SDL_Color col = (w == 0) ? SDL_Color{220, 30, 30, 255}
                                            : SDL_Color{30, 80, 220, 255};
            DrawText(renderer, fontTitle,
                     w == 0 ? "P1 WINS THE MATCH" : "P2 WINS THE MATCH",
                     kScreenW / 2, 320, col, true);
            DrawText(renderer, fontRegular, "Press any key to return to title",
                     kScreenW / 2, 420, SDL_Color{220, 220, 220, 255}, true);
        }

        // HUD 점수 텍스트 — HP 바/dot(중앙 y≈28) 바로 아래에 배치해 색상 dot
        // 채움을 가리지 않도록 한다 (y=52, dot/HP바 영역 16~40 아래).
        if (fontHud) {
            char score[16];
            std::snprintf(score, sizeof(score), "%d  -  %d",
                          ctx.flow.P1Score(), ctx.flow.P2Score());
            DrawText(renderer, fontHud, score, kScreenW / 2, 52,
                     SDL_Color{220, 220, 220, 255}, true);
        }

        // F7 — 풀 점유 수 TTF 텍스트 (debug build: drawPoolUsage 토글 시 표시).
        //   PhysicsWorld.BodyCount() / ProjectilePool.Storage() / ChunkSpawner.Storage()
        //   의 살아있는 항목 수를 화면 좌상단 (10, 10) 근처에 작은 텍스트로 표시.
        //   폰트가 로드되지 않았으면 no-op (ResourceManager miss).
        if (debugOpts.drawPoolUsage && fontHud) {
            // body: AliveCount() 대신 BodyCount() 사용 (PhysicsWorld API).
            const int bodyAlive = ctx.world.BodyCount();
            // projectile: Pool ForEachAlive 카운트 (Storage().Alive() 공개 API 없음).
            int projAlive = 0;
            ctx.projectiles.Storage().ForEachAlive(
                [&](int, Projectile&) { ++projAlive; });
            // chunk: ChunkSpawner 의 Pool ForEachAlive 카운트.
            int chunkAlive = 0;
            if (ctx.chunks) {
                ctx.chunks->Storage().ForEachAlive(
                    [&](int, Chunk&) { ++chunkAlive; });
            }
            char poolBuf[64];
            std::snprintf(poolBuf, sizeof(poolBuf),
                          "Bodies:%d  Proj:%d  Chunks:%d",
                          bodyAlive, projAlive, chunkAlive);
            DrawText(renderer, fontHud, poolBuf,
                     10, 10, SDL_Color{180, 255, 180, 255}, false);
        }

#ifdef CRUMBLE_DEBUG
        // F8 — 런타임 튜닝 파라미터 오버레이 (debug build only).
        //   현재 선택 파라미터 이름 + 값을 화면 좌상단 두 번째 줄에 표시.
        //   Tab: 파라미터 순환 / Left/Right: 값 조정.
        if (debugOpts.drawTuningSliders && fontHud) {
            // 파라미터 이름 배열 — TuningParam 열거형 순서와 일치.
            const char* kParamNames[] = {"Gravity", "Restitution", "PlayerSpeed"};
            // 현재 선택 파라미터 값 읽기.
            float curVal = 0.0f;
            if (tuningIdx == static_cast<int>(TuningParam::Gravity))
                curVal = tuningGravity;
            else if (tuningIdx == static_cast<int>(TuningParam::Restitution))
                curVal = tuningRestitution;
            else if (tuningIdx == static_cast<int>(TuningParam::PlayerSpeed))
                curVal = tuningPlayerSpeed;

            char tuneBuf[64];
            std::snprintf(tuneBuf, sizeof(tuneBuf),
                          "[F8] %s: %.2f  Tab/</>/\\>",
                          kParamNames[tuningIdx], curVal);
            // F7 pool stats 아래 두 번째 줄 (y=30) 에 노란색으로 표시.
            DrawText(renderer, fontHud, tuneBuf,
                     10, 30, SDL_Color{255, 255, 100, 255}, false);
        }
#endif

        // P10-B Task 7 — 전체 화면 페이드 오버레이.
        //   fadeAlpha > 1 일 때만 그려 alpha=0 시 SDL 호출 절약.
        //   BLEND 모드로 반투명 검은 사각형을 화면 위에 덮음.
        if (fadeAlpha > 1.0f) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0,
                                   static_cast<Uint8>(fadeAlpha));
            SDL_Rect full = {0, 0, kScreenW, kScreenH};
            SDL_RenderFillRect(renderer, &full);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }

        SDL_RenderPresent(renderer);

        // FPS 1초마다 타이틀 표시 (debug 빌드에서만 가독성 의미 — release 도
        // 별 비용 없으므로 항상 갱신).
        if (fpsTimer >= 1.0f) {
            char title[96];
            std::snprintf(title, sizeof(title),
                          "Crumble  |  %d fps  |  P1 %d  P2 %d",
                          frameCount,
                          ctx.flow.P1Score(),
                          ctx.flow.P2Score());
            SDL_SetWindowTitle(window, title);
            frameCount = 0;
            fpsTimer   = 0.0f;
        }
    }

    // ===== cleanup ===========================================================
    //   SDL_*Quit / Mix_CloseAudio / TTF_Quit / IMG_Quit 은 RAII guards 의
    //   소멸자가 main() 종료 시 자동 호출 — 본 자리에서는 window/renderer 만
    //   수동 destroy.
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    return 0;
} catch (const std::exception& e) {
    // RAII guards 가 throw 한 init 실패 catch — stderr 로 메시지 + 비정상 종료.
    std::fprintf(stderr, "[main] fatal: %s\n", e.what());
    return 1;
}
