# =============================================================================
# Crumble — top-level CMake build script
#
# 역할: SDL2 family 를 찾아서 게임 본체 (`crumble`) 와 단위 테스트 러너
#       (`crumble_tests`) 두 개의 실행 파일을 만든다.
#
# 출처 / 참고:
#   - [3. Game Loop.pdf p.2-3] 최소 SDL setup (Init / CreateWindow /
#     CreateRenderer / 종료 역순) 이 본 빌드의 기본 의존이다.
#   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md 의 P0 Task 0.2 와
#     "Performance Budget & Threading Discipline" 섹션 (-ffast-math 금지 규칙).
#
# 빌드 명령:
#   Debug   (F8 tuning + F12 snapshot 활성화):
#     cmake -S . -B build-debug   -DCMAKE_BUILD_TYPE=Debug
#     cmake --build build-debug   -j
#   Release (발표용 바이너리):
#     cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
#     cmake --build build-release -j
#
#   테스트 실행:
#     ./build-debug/crumble_tests              (단독 실행, [PASS]/[FAIL] 줄 출력)
#     ctest --test-dir build-debug             (CTest 통합)
# =============================================================================

# CMake 3.16: Ubuntu 20.04 / Debian 11 기본 패키지 버전과 호환되는 최저선.
# 더 낮춰도 동작은 하지만 SDL2_image::SDL2_image 같은 IMPORTED target 표기가
# 흔들릴 수 있어 보수적으로 3.16 을 잡았다.
cmake_minimum_required(VERSION 3.16)

# project() 는 컴파일러 식별을 트리거. CXX 만 선언해 C 컴파일러는 SDL2 의
# C 헤더만 거치고 우리 코드는 모두 C++ 로 일원화한다.
project(crumble CXX)

# -----------------------------------------------------------------------------
# C++ 표준 — C++17 (메인 플랜의 Tech Stack 명세).
# -----------------------------------------------------------------------------
# C++17 을 고른 이유:
#   - 강의 PDF 가 std::chrono 등 C++11~17 범위 기능만 사용 (PDF 후반부에 더
#     높은 표준을 전제하는 부분 없음).
#   - C++20 의 modules / coroutines / std::format 은 GCC/Clang 의 텀프로젝트
#     수준 환경에서 ABI 호환 이슈 가능성. 안전하게 17.
#   - C++17 의 structured bindings + std::optional + if constexpr 면 본 게임
#     난이도에는 충분.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# extension 끄기 = `-std=c++17` 사용 (`-std=gnu++17` 가 아니라). 컴파일러간
# 동작 차이를 줄여 결정론에 도움.
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# 결정론 보호: -ffast-math 금지.
# -----------------------------------------------------------------------------
# 메인 플랜의 비협상 불변식 ("결정론 물리 코어") 을 지키려면 부동소수점 환경
# 을 건드리지 않아야 한다. -ffast-math 는 (a+b)+c == a+(b+c) 같은 결합법칙을
# 가정해 inf/NaN 처리를 단축시키므로, 같은 입력에 결과 해시가 달라질 수 있다.
# 본 빌드에서는 add_compile_options 에 절대 추가하지 않는다.
if(NOT MSVC)
    # -Wall -Wextra -Wpedantic: 강의 수준 코드의 일반적 기본값. 프로젝트 진행
    # 중 경고가 늘어나는 것을 막아 발표 직전 깔끔한 빌드를 보장한다.
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# -----------------------------------------------------------------------------
# 외부 의존: SDL2 family.
# -----------------------------------------------------------------------------
# Linux/WSL 사전 설치 명령:
#   sudo apt install cmake pkg-config \
#                    libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev
#
# 각 라이브러리 역할:
#   SDL2        - 창 / 렌더러 / 입력 / 타이머 ([3. Game Loop.pdf p.3] 핵심)
#   SDL2_image  - PNG 텍스처 로드 (P9 ResourceManager 가 사용)
#   SDL2_mixer  - WAV / OGG 사운드 (P9 AudioManager 가 사용)
#   SDL2_ttf    - 비트맵 폰트 렌더 (P3 HUD 점수 텍스트, P9 메뉴 텍스트)
#
# === 의존 검색 방식 분기 이유 (중요) ===
# Ubuntu 22.04 의 libsdl2-{image,mixer,ttf}-dev 패키지는 sdl2-config + pkg-config
# 메타파일만 설치하고 SDL2_imageConfig.cmake 같은 CMake 패키지 설정 파일을
# 제공하지 않는다. 따라서 find_package(SDL2_image REQUIRED) 가 즉시 실패한다.
#
# 본 빌드는 다음 두 단계로 의존을 잡는다:
#   (1) SDL2 본체     - sdl2-config 가 SDL2Config.cmake 를 제공하므로 find_package
#                       으로 그대로 잡는다.
#   (2) image/mixer/ttf - PkgConfig 로 IMPORTED 타겟을 만들어 가져온다 — 어떤
#                       리눅스 배포판이든 pkg-config 메타파일은 표준으로
#                       제공되므로 이식성이 더 좋다.
#
# Plan Revisability Policy 노트: 학기 후반 PDF 가 SDL2 의존 검색을 다른 방식
# (예: vcpkg, FetchContent) 으로 가르치면 그 패턴으로 정렬한다. 본 형태는
# 현재 시점 (2026-05-06) Ubuntu/WSL 환경에서 root-cause 수정이며 워크어라운드
# 가 아니다.
# pthreads (ThreadPool / AsyncLoader 가 std::thread 를 사용하므로 명시적 링크 필요).
# Linux / WSL 에서 std::thread 는 -lpthread 없이 링크하면 undefined reference.
find_package(Threads REQUIRED)

# -----------------------------------------------------------------------------
# SDL2 family 검색 — 플랫폼별 분기 (cross-platform: Windows/PowerShell + Linux/WSL)
# -----------------------------------------------------------------------------
# Windows (PowerShell, vcpkg 권장):
#     vcpkg install sdl2 sdl2-image sdl2-mixer sdl2-ttf
#     cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
#   vcpkg 가 SDL2*Config.cmake 를 제공하므로 find_package(... CONFIG) 가 IMPORTED
#   타겟(SDL2::SDL2 / SDL2_image::SDL2_image …)을 만들어 include/lib 를 transitive
#   하게 전파한다. SDL2::SDL2main 은 링크하지 않는다 — main 진입점은 코드에서
#   SDL_MAIN_HANDLED + SDL_SetMainReady() 로 직접 관리(파일 하단 compile-def 참조).
#
# Linux / WSL:
#   Ubuntu 의 libsdl2-{image,mixer,ttf}-dev 는 *Config.cmake 를 제공하지 않으므로
#   find_package(SDL2_image) 가 실패한다 → pkg-config(IMPORTED_TARGET) 로 잡는다.
#   배포판별 root-cause 대응이며 워크어라운드가 아니다.
if(WIN32)
    find_package(SDL2       CONFIG REQUIRED)
    find_package(SDL2_image CONFIG REQUIRED)
    find_package(SDL2_mixer CONFIG REQUIRED)
    find_package(SDL2_ttf   CONFIG REQUIRED)
    set(CRUMBLE_SDL2_CORE    SDL2::SDL2)
    set(CRUMBLE_SDL2_EXTRAS  SDL2_image::SDL2_image SDL2_mixer::SDL2_mixer SDL2_ttf::SDL2_ttf)
    set(CRUMBLE_SDL2_INCLUDE "")    # CONFIG 타겟이 include 를 transitive 전파.
else()
    find_package(SDL2 REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)
    pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET SDL2_mixer)
    pkg_check_modules(SDL2_TTF   REQUIRED IMPORTED_TARGET SDL2_ttf)
    set(CRUMBLE_SDL2_CORE    ${SDL2_LIBRARIES})
    set(CRUMBLE_SDL2_EXTRAS  PkgConfig::SDL2_IMAGE PkgConfig::SDL2_MIXER PkgConfig::SDL2_TTF)
    set(CRUMBLE_SDL2_INCLUDE ${SDL2_INCLUDE_DIRS})
endif()

# -----------------------------------------------------------------------------
# 디버그 빌드 전용 매크로: CRUMBLE_DEBUG=1
# -----------------------------------------------------------------------------
# F8 (런타임 튜닝 슬라이더) 와 F12 (스냅샷 ThreadPool 잡) 는 디버그 빌드에서만
# 노출된다 — 발표용 Release 바이너리에는 들어가지 않는다 (P10 Acceptance
# Gate 요건). $<$<CONFIG:Debug>:...> 제너레이터 표현식은 빌드 타입이 Debug
# 일 때만 매크로를 활성화.
add_compile_definitions($<$<CONFIG:Debug>:CRUMBLE_DEBUG=1>)

# -----------------------------------------------------------------------------
# 타겟 1: 게임 본체 `crumble`
# -----------------------------------------------------------------------------
# P1 Task 1.6 시점: src/main.cpp 가 PhysicsWorld + 5 boxes stacking 데모로
# 갱신됨에 따라 Vec2 / PhysicsBody / Collision / Resolution / PhysicsWorld
# 를 함께 컴파일해야 한다. 이후 phase 마다 본 목록이 계속 누적된다.
add_executable(crumble
    src/main.cpp
    src/core/Timer.cpp
    src/core/CooldownTimer.cpp
    src/math/Vec2.cpp
    src/physics/PhysicsBody.cpp
    src/physics/Collision.cpp
    src/physics/Resolution.cpp
    src/physics/PhysicsWorld.cpp
    src/physics/SpatialGrid.cpp
    src/input/InputManager.cpp
    src/input/Bindings.cpp
    src/game/Player.cpp
    src/game/Projectile.cpp
    src/game/CombatHooks.cpp
    src/game/weapons/Rifle.cpp
    src/game/weapons/Shotgun.cpp
    src/game/weapons/MortarAimState.cpp
    src/game/weapons/Mortar.cpp
    src/game/Map.cpp
    src/game/MovingPlatform.cpp
    src/game/TileGrid.cpp
    src/game/BreakableSlab.cpp
    src/game/ChunkSpawner.cpp
    src/game/MapEditor.cpp
    src/flow/GameFlow.cpp
    src/render/HealthBar.cpp
    src/render/HUD.cpp
    src/render/Text.cpp
    src/resource/ResourceManager.cpp
    src/resource/AsyncLoader.cpp
    src/resource/ThreadPool.cpp
    src/audio/AudioManager.cpp
    src/physics/DebugDraw.cpp
)

# 헤더 검색 경로:
#   - src/         프로젝트 자체 헤더. `#include "core/Timer.h"` 가능.
#   - third_party/ nlohmann/json 같은 single-header 외부 라이브러리 (P9).
#   - ${SDL2_INCLUDE_DIRS} SDL2 의 시스템 설치 경로.
target_include_directories(crumble PRIVATE
    src
    third_party
    ${CRUMBLE_SDL2_INCLUDE}
)

# 링크: SDL2 본체 + image/mixer/ttf 보조 라이브러리.
# - SDL2 본체는 배포판마다 IMPORTED target 이름이 다를 수 있어 ${SDL2_LIBRARIES}
#   변수 형식을 사용 (Ubuntu 의 sdl2-config 출력이 이 변수에 들어간다).
# - image/mixer/ttf 는 위 pkg_check_modules 의 IMPORTED_TARGET 옵션이 만든
#   PkgConfig::SDL2_IMAGE 형식의 모던 CMake 타겟을 사용 — 이 타겟은 헤더 경로,
#   라이브러리 경로, 컴파일 플래그를 모두 transitive 하게 전파해준다.
target_link_libraries(crumble PRIVATE
    ${CRUMBLE_SDL2_CORE}
    ${CRUMBLE_SDL2_EXTRAS}
    Threads::Threads
)

# -----------------------------------------------------------------------------
# 타겟 2: 단위 테스트 러너 `crumble_tests`
# -----------------------------------------------------------------------------
# enable_testing() 는 ctest 명령이 동작하도록 CTest 통합을 켠다. add_test 가
# 없어도 호출은 안전.
enable_testing()

# 테스트 러너 소스 구성 (P0 Task 0.4 에서 채워짐):
#   tests/test_main.cpp           — 핸드롤 테스트 러너 + REGISTER_TEST 매크로
#   tests/test_cooldown_timer.cpp — CooldownTimer 4 케이스
#   src/core/CooldownTimer.cpp    — 검증 대상 구현 (테스트 바이너리에 직접 링크)
#
# 검증 대상 .cpp 를 테스트 바이너리에 같이 컴파일하는 패턴 (정적 라이브러리
# 분리 안 함) 인 이유: 본 프로젝트 규모에서 라이브러리 분리는 빌드 시간을
# 늘리고 테스트 격리에 별 이득이 없다. phase 가 늘면 필요시 CMake OBJECT
# 라이브러리로 묶어 중복 컴파일을 줄이는 것도 가능 (P10 폴리싱 시 검토).
add_executable(crumble_tests
    tests/test_main.cpp
    tests/test_cooldown_timer.cpp
    tests/test_vec2.cpp
    tests/test_physics_body.cpp
    tests/test_collision_aabb.cpp
    tests/test_resolution.cpp
    tests/test_physics_world.cpp
    tests/test_input_action_map.cpp
    tests/test_player_fsm.cpp
    tests/test_pool.cpp
    tests/test_weapon_reload.cpp
    tests/test_game_flow.cpp
    tests/test_collision_circle.cpp
    tests/test_collision_aabb_circle.cpp
    tests/test_determinism_replay.cpp
    tests/test_mortar_aimstate.cpp
    tests/test_spatial_grid.cpp
    tests/test_tile_grid.cpp
    tests/test_math_utils.cpp
    tests/test_threadpool.cpp
    src/core/CooldownTimer.cpp
    src/math/Vec2.cpp
    src/physics/PhysicsBody.cpp
    src/physics/Collision.cpp
    src/physics/Resolution.cpp
    src/physics/PhysicsWorld.cpp
    src/physics/SpatialGrid.cpp
    src/input/InputManager.cpp
    src/input/Bindings.cpp
    src/game/Player.cpp
    src/game/Projectile.cpp
    src/game/weapons/Rifle.cpp
    src/game/weapons/Shotgun.cpp
    src/game/weapons/MortarAimState.cpp
    src/game/weapons/Mortar.cpp
    src/game/Map.cpp
    src/game/TileGrid.cpp
    src/flow/GameFlow.cpp
    src/resource/ThreadPool.cpp
)
# 테스트 바이너리는 SDL2 헤더만 (창 / 렌더 호출 없음). InputManager 가
# SDL_GetKeyboardState 를 참조하지만 SimulateScancode 만 사용하는 테스트
# 에서는 호출되지 않으므로 SDL 라이브러리 링크는 필요 없을 것 같지만,
# SDL_NUM_SCANCODES 매크로 + Uint8 등의 심볼 의존이 있어 SDL2 link 추가.
target_link_libraries(crumble_tests PRIVATE ${CRUMBLE_SDL2_CORE} Threads::Threads)
target_include_directories(crumble_tests PRIVATE ${CRUMBLE_SDL2_INCLUDE})
# tests/ 를 인클루드 경로에 추가 — test_*.cpp 들이 같은 디렉터리의
# test_runner.h 를 "test_runner.h" (꺽쇠 아닌 따옴표) 로 가져오기 위함.
target_include_directories(crumble_tests PRIVATE src third_party tests)

# CTest 가 인식하는 단위 테스트 등록.
# `ctest --test-dir build-debug` 명령 시 이 이름의 테스트가 실행된다.
add_test(NAME unit COMMAND crumble_tests)

# -----------------------------------------------------------------------------
# Windows 진입점 처리 — SDL_MAIN_HANDLED
# -----------------------------------------------------------------------------
# Windows 에서 <SDL.h> 는 기본적으로 main 을 SDL_main 으로 재정의하고 SDL2main 이
# 제공하는 WinMain 을 진입점으로 쓴다. 본 프로젝트는 SDL2main 에 의존하지 않고
# SDL_MAIN_HANDLED 를 정의해 표준 main() 을 그대로 진입점으로 사용하며, main()
# 첫 줄에서 SDL_SetMainReady() 를 호출한다 (게임/테스트 두 콘솔 앱 모두 동일).
# Linux/WSL 에서는 main 재정의가 없어 본 정의가 불필요하므로 WIN32 에서만 적용.
if(WIN32)
    target_compile_definitions(crumble       PRIVATE SDL_MAIN_HANDLED)
    target_compile_definitions(crumble_tests PRIVATE SDL_MAIN_HANDLED)
endif()
