// =============================================================================
// src/render/RenderUtils.h — 렌더링 유틸리티 (SDL 색상 보간 등).
//
// 목적:
//   - SDL_Color 타입에 의존하는 렌더링 보조 함수를 제공.
//   - 레이어 분리 원칙: SDL 헤더를 포함하는 코드는 render/ 레이어에 위치.
//     순수 수학 함수(Lerp, Remap 등)는 SDL 없는 math/MathUtils.h 에 있으며,
//     여기서는 그것을 가져다 쓰는 SDL 전용 래퍼만 둔다.
//
// 출처:
//   - [8. Numerical Analysis 1.pdf p.2] — 선형 보간(Lerp) 원리.
//   - SDL2 공식 문서 — SDL_Color 구조체 (r, g, b, a 각 uint8_t).
//
// 결정론:
//   - 색상 보간은 HUD / 애니메이션 레이어에서만 호출되며 PhysicsWorld::Step 과
//     무관하다 — 시뮬레이션 결정론에 영향 없음.
//
// 헤더 단독:
//   - 모든 함수 inline. .cpp 파일 없음.
// =============================================================================

#pragma once

#include <SDL.h>          // SDL_Color (r, g, b, a 각 Uint8).
#include <algorithm>      // std::clamp
#include "../math/MathUtils.h"  // Lerp(float, float, float)

// LerpColor — 두 SDL_Color 를 t 로 선형 보간.
//   [8. Numerical Analysis 1.pdf p.2] 의 Lerp 를 각 채널(r, g, b, a) 에 적용.
//
//   - t=0.0f 이면 c1 반환, t=1.0f 이면 c2 반환.
//   - 각 채널은 Lerp 후 [0, 255] 로 clamp 해 uint8 오버플로 방지.
//   - 사용 예:
//       HP 가 높을 때 녹색, 낮을 때 빨간색으로 점진적 전환.
//       라운드 전환 시 검은색 페이드인/아웃.
//
//   파라미터:
//     c1  — t=0 일 때의 색상.
//     c2  — t=1 일 때의 색상.
//     t   — 보간 계수. [0, 1] 권장 (범위 밖이면 외삽).
//
//   반환:
//     채널별 보간된 SDL_Color.
inline SDL_Color LerpColor(SDL_Color c1, SDL_Color c2, float t) {
    // 각 채널을 float 로 보간한 뒤 [0, 255] 클램프 후 uint8 로 변환.
    auto lerpCh = [t](Uint8 a, Uint8 b) -> Uint8 {
        float v = Lerp(static_cast<float>(a), static_cast<float>(b), t);
        // std::clamp 로 [0, 255] 보장 (t 가 [0,1] 밖이어도 안전).
        return static_cast<Uint8>(std::clamp(v, 0.0f, 255.0f));
    };
    return SDL_Color{
        lerpCh(c1.r, c2.r),
        lerpCh(c1.g, c2.g),
        lerpCh(c1.b, c2.b),
        lerpCh(c1.a, c2.a)
    };
}
