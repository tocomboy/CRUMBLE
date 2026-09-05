# Crumble

2D 사이드뷰 PvP 슈터 (1v1 로컬, 5라운드 3선승).
직접 구현한 **2D 물리 엔진**(Static/Kinematic/Dynamic 강체, Symplectic Euler 적분, Spatial-Grid broad-phase, impulse 기반 충돌 응답, 파괴 가능 지형)을 중심으로 한다. 캐릭터·무기는 PNG 스프라이트로, 지형·투사체는 도형으로 렌더한다.

- **Stack**: C++17 + SDL2 / SDL2_image / SDL2_mixer / SDL2_ttf, nlohmann/json (single-header), CMake
- **플랫폼**: Linux / WSL **및 Windows(PowerShell)** 모두 빌드·실행 가능 (CMake cross-platform)
- **단위 테스트**: assert 기반 자체 러너와 CTest 제공

## 시작하기

```bash
git clone https://github.com/tocomboy/CRUMBLE.git
cd CRUMBLE
```

이후 명령은 저장소 루트에서 실행합니다. 게임은 현재 작업 디렉터리의 `assets/`를 읽습니다.
CMake 3.16 이상과 C++17 컴파일러가 필요합니다. WSL에서 창을 띄우려면 WSLg 등의 그래픽 환경이 필요합니다.

## 1. 의존성 설치

### 1-A. Linux / WSL

```bash
sudo apt install libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev cmake g++ pkg-config
```

### 1-B. Windows (PowerShell) — vcpkg 권장

Visual Studio 2022 또는 Build Tools의 **C++를 사용한 데스크톱 개발**과 CMake를 준비합니다.
vcpkg는 저장소 바깥에 설치합니다. 아래 설치 예시는 저장소의 상위 폴더에서 실행합니다.

```powershell
cd ..
# 1) vcpkg 설치 (최초 1회)
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat

# 2) SDL2 family 설치 (x64)
.\vcpkg\vcpkg install sdl2 sdl2-image sdl2-mixer sdl2-ttf --triplet x64-windows
cd CRUMBLE
```

CMake 가 플랫폼을 자동 감지한다 — Windows 에서는 `find_package(... CONFIG)`(vcpkg), Linux 에서는 pkg-config 로 SDL2 를 찾는다. 진입점은 `SDL_MAIN_HANDLED` + `SDL_SetMainReady()` 로 직접 관리하므로 SDL2main 의존이 없다.

`third_party/json.hpp` (nlohmann/json single-header) 는 동봉되어 있어 별도 설치가 필요 없다.

## 2. 빌드

### Linux / WSL

```bash
# Debug (디버그 오버레이 F1~F12 + 런타임 튜닝 + F12 스냅샷 활성)
cmake -S . -B build-debug   -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug   -j

# Release (일반 실행용)
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
```

### Windows (PowerShell)

```powershell
# <vcpkg> 는 위에서 clone 한 경로. 툴체인 파일만 지정하면 끝.
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE="<vcpkg>/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
# 실행: .\build\Release\crumble.exe  (테스트: .\build\Release\crumble_tests.exe)
```

> Visual Studio 로 열 때도 "폴더 열기"(CMake)로 같은 `CMakeLists.txt` 를 그대로 사용하며,
> vcpkg 툴체인을 CMake 설정에 지정하면 된다.

## 3. 실행

```bash
./build-release/crumble      # 일반 실행용
./build-debug/crumble        # 디버그 오버레이 사용 가능
```

## 4. 조작

| 동작 | Player 1 | Player 2 |
|------|----------|----------|
| 좌/우 이동 | `A` / `D` | `←` / `→` |
| 점프 | `W` | `↑` |
| 하강 | `S` | `↓` |
| 무기 1/2/3 (라이플/샷건/박격포) | `1` `2` `3` | `KP 1` `KP 2` `KP 3` |
| 발사 | `F` | `KP 0` |
| 박격포 각도 토글 | `G` | `KP Enter` |

게임 내 **Controls** 화면에서도 키 안내를 볼 수 있다.

## 4-1. 맵 에디터 (마우스로 맵 직접 제작)

타이틀 화면에서 **`E`** 키를 누르면 마우스 맵 에디터로 진입한다. 저장한 JSON 은
게임이 그대로 읽어 플레이된다 (F1~F3 으로 기존 3맵을 불러와 편집 후 `S` 로 덮어쓰기 가능).

| 키 / 입력 | 기능 |
|-----------|------|
| `1`~`8` | 배치할 종류 선택 — 1 정적벽 · 2 파괴타일 · 3 긴파괴벽(slab) · 4 cover(소) · 5 cover(대) · 6 P1 스폰 · 7 P2 스폰 · 8 이동발판 |
| 좌클릭 드래그 | 사각형 배치 (16px 그리드 스냅) |
| 좌클릭 (스폰 선택 시) | 스폰 지점 지정 |
| 우클릭 | 커서 위 오브젝트 삭제 |
| `F1` `F2` `F3` | Broken Bridge / Bunker / Hideout 불러오기 |
| `S` | 현재 파일로 저장 (미로드 시 `assets/maps/custom.json`) |
| `N` | 새 빈 맵 (외곽 벽 자동) |
| `G` | 그리드 토글 |
| `ESC` | 타이틀로 복귀 |

## 5. 테스트

```bash
./build-debug/crumble_tests        # [PASS]/[FAIL] 라인별 출력
ctest --test-dir build-debug       # CTest 집계
```

## 6. 디버그 오버레이 (Debug 빌드, Playing 상태에서)

| 키 | 기능 |
|----|------|
| `F1` | body 히트박스 (색 = body 타입) |
| `F2` | contact normal (빨강 화살표) |
| `F3` | impulse 벡터 (velocity-as-impulse proxy, 녹색, 0.05x) |
| `F4` | broad-phase grid cell |
| `F5` | step당 contact pair 수 |
| `F6` | velocity heading 벡터 (파랑) |
| `F7` | pool 점유 텍스트 (bodies / projectiles / chunks) |
| `F8` | 런타임 튜닝 슬라이더 (Debug 전용) |
| `F9` | 랜덤 원 50개 스트레스 스폰 (성능 테스트) |
| `F12` | ThreadPool 경유 비동기 스냅샷 → `snapshot.json` (Debug 전용) |

## 7. 프로젝트 구조

```
src/
  math/        Vec2, MathUtils (Lerp/Easing/Remap)
  physics/     PhysicsBody, PhysicsWorld(Step), Collision(narrow), Resolution(impulse),
               SpatialGrid(broad-phase), DebugDraw
  input/       InputManager (action map, scancode), Bindings
  game/        Player(FSM), Weapon/Projectile, weapons/(Rifle/Shotgun/Mortar),
               TileGrid(파괴 지형), ChunkSpawner(파편), Map(JSON), MovingPlatform, CombatHooks
  flow/        GameFlow FSM (Title→Controls→RoundIntro→Playing→RoundEnd→MatchEnd)
  render/      HUD, HealthBar, Text(TTF)
  resource/    ResourceManager, AsyncLoader, ThreadPool, Pool
  core/        Timer, CooldownTimer
  audio/       AudioManager
tests/         assert 기반 단위 테스트 (REGISTER_TEST)
assets/        maps(JSON) / fonts(TTF) / sfx / bgm / sprites(PNG)
third_party/   json.hpp (nlohmann/json)
licenses/      외부 리소스 라이선스 전문
THIRD_PARTY_NOTICES.md   외부 라이브러리·리소스 출처
LICENSE        프로젝트 MIT 라이선스
```

## 8. 설계 노트

- **물리 코어는 단일 스레드 결정론**: `PhysicsWorld::Step` 은 메인 스레드 전용. ThreadPool/AsyncLoader 는 에셋 로딩·스냅샷 등 비시뮬레이션 잡에만 사용.
- **적분기 = Symplectic(semi-implicit) Euler**: 속도를 먼저 갱신하고 그 속도로 위치를 적분 → 에너지 보존, 게임 물리의 최소 안정 기준선.
- **시각/충돌 1:1 매핑**: 지형·투사체는 도형(원/AABB), 캐릭터·무기는 PNG 스프라이트로 렌더하되, 충돌체와 그려지는 영역을 정확히 일치시켜 "보이지 않는 벽"이 없도록 한다. 파괴 지형은 한 벽처럼 솔리드하게 그리고, 파괴 불가능한 벽과는 색으로 구분한다.
- **결정론 유지**: `-ffast-math` 미사용, FP 환경 불변. P4 replay 테스트로 검증.

## 9. 라이선스

프로젝트 자체 코드, 맵·스프라이트와 자체 합성 효과음은 [MIT 라이선스](LICENSE)로 공개합니다.
외부 라이브러리, 글꼴, 음원에는 각각 원래 라이선스가 적용됩니다.
출처와 조건은 [외부 리소스 고지](THIRD_PARTY_NOTICES.md)를 확인하세요.

이 저장소는 소스코드, 빌드·검증 파일과 필수 리소스를 제공합니다. 수업 자료와 개인 문서는 포함하지 않습니다.
소스 주석의 강의·개발 문서 참조는 구현 배경이며, 해당 문서는 빌드나 실행에 필요하지 않습니다.
