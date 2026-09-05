// =============================================================================
// src/physics/DebugDraw.h — 물리 디버그 오버레이 (F1~F8 후크).
// =============================================================================
//
// 본 P4 단계의 활성 범위:
//   - F1 hitboxes        : body 외곽선 (Static 회색 / Kinematic 파랑 / Dynamic 노랑).
//   - F2 contact normals : 직전 step 의 contact normal 빨간 화살표.
//   - F3 impulses        : 본 단계는 vel proxy (초록 화살표) — 본격 impulse
//                          시각화는 P10 단계의 F8 슬라이더와 함께 보강.
//   - F4~F8 옵션은 enum/필드로 미리 정의해 두지만 본 P4 에서는 그리지 않음.
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 4 Task 4.4.
//   - 메인 스펙 §6 Debug HUD 의 키 매핑.
//
// 결정론 / 스레드:
//   - 본 모듈은 read-only — PhysicsBody / Contact 의 const view 만 읽고 SDL
//     렌더 호출만 수행. 시뮬레이션 결과에 영향 없음 → 결정론 안전.
// =============================================================================

#pragma once

#include <SDL.h>

class PhysicsWorld;

// DebugDrawOptions: 키 토글로 켜고 끄는 오버레이 항목들.
//   - F1~F3 본 P4 활성, 나머지는 후속 phase 에서 활성.
//   - 모든 필드 default false → 기본 빌드는 깨끗한 화면.
struct DebugDrawOptions {
    bool drawHitboxes      = false;   // F1 P4
    bool drawNormals       = false;   // F2 P4
    bool drawImpulses      = false;   // F3 P4 (vel proxy)
    bool drawGrid          = false;   // F4 P7 (SpatialGrid 활성화 후)
    bool drawPairs         = false;   // F5 P7
    bool drawVelocities    = false;   // F6 P10
    bool drawPoolUsage     = false;   // F7 P10
    bool drawTuningSliders = false;   // F8 P10 (debug build only)
};

// DrawPhysicsDebug: 한 번의 호출로 켜진 옵션을 모두 그린다.
//   - r     : 게임 SDL_Renderer.
//   - world : 살아있는 PhysicsBody 목록 + LastContacts 접근.
//   - opts  : 키 토글 상태.
// 호출 시점: 메인 frame 의 정적 / Player / Projectile / HUD 렌더 사이 (HUD
// 위에 그리면 HP 바 위로 화살표가 가려져 가독성 ↓ — 메인 플랜 권장은 projectile
// 렌더 직후, HUD 직전).
void DrawPhysicsDebug(SDL_Renderer* r, PhysicsWorld& world, const DebugDrawOptions& opts);
