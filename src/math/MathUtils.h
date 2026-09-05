// =============================================================================
// src/math/MathUtils.h — 수학 유틸리티 (Lerp, Remap, WrapAngle, 이징 함수).
//
// 목적:
//   - 게임에서 자주 쓰이는 보간(interpolation), 범위 재매핑(remap), 각도 정규화,
//     이징(easing) 함수를 한 곳에 모아 재사용성을 높인다.
//   - SDL 의존 없는 순수 수학 레이어 — <cmath> 와 Vec2.h 만 포함.
//     SDL 색상 보간이 필요하면 src/render/RenderUtils.h 를 사용할 것.
//
// 출처:
//   - [8. Numerical Analysis 1.pdf p.9] — 선형 보간(Lerp), InverseLerp, Remap
//   - [8. Numerical Analysis 1.pdf p.5] — WrapAngle, 각도 정규화
//   - [8. Numerical Analysis 1.pdf p.10] — 이징 곡선 (Ease namespace)
//
// 결정론:
//   - 모든 연산은 IEEE-754 표준 float. -ffast-math 금지.
//   - fmod, fabs, sin, pow 는 표준 <cmath> 구현 사용.
//   - 이 헤더의 함수들은 PhysicsWorld::Step 외부 (HUD, 애니메이션, 오디오 페이드)
//     에서만 호출된다 — 시뮬레이션 결정론에 영향 없음.
//
// 헤더 단독:
//   - 모든 함수 inline. .cpp 파일 없음.
//   - SDL 을 포함하지 않으므로 테스트 바이너리에서도 SDL 초기화 없이 사용 가능.
// =============================================================================

#pragma once

#include <cmath>      // std::fabs, std::fmod, std::sin, std::pow
#include "Vec2.h"     // Vec2, Lerp(Vec2) 에 사용

// -----------------------------------------------------------------------------
// 선형 보간 (Linear Interpolation)
// -----------------------------------------------------------------------------

// Lerp — 스칼라 선형 보간.
//   [8. Numerical Analysis 1.pdf p.9]
//   - 공식: a + (b - a) * t.
//   - t=0 이면 a, t=1 이면 b 를 반환.
//   - t 를 [0,1] 밖으로 두면 외삽(extrapolation) 이 되므로 호출자가 클램프 필요.
inline float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// LerpVec2 — Vec2 선형 보간.
//   [8. Numerical Analysis 1.pdf p.9]
//   - 각 성분에 Lerp 적용. 위치 / 속도 보간에 사용.
inline Vec2 LerpVec2(const Vec2& a, const Vec2& b, float t) {
    return {Lerp(a.x, b.x, t), Lerp(a.y, b.y, t)};
}

// InverseLerp — Lerp 의 역연산. v 가 [a, b] 구간 내 어디에 있는지 0~1 로 반환.
//   [8. Numerical Analysis 1.pdf p.9]
//   - 공식: (v - a) / (b - a).
//   - b == a 이면 (구간 길이 0) 0 반환 — 분모 0 방지.
//   - Remap 의 내부 구현에 사용.
inline float InverseLerp(float a, float b, float v) {
    float d = b - a;
    if (std::fabs(d) < 1e-9f) return 0.0f;  // 구간 없음 — 0 반환.
    return (v - a) / d;
}

// Remap — 입력 범위 [inLo, inHi] 의 값 v 를 출력 범위 [outLo, outHi] 로 재매핑.
//   [8. Numerical Analysis 1.pdf p.9]
//   - 공식: Lerp(outLo, outHi, InverseLerp(inLo, inHi, v)).
//   - 예: HP 0~100 을 게이지 길이 0~200 픽셀로 변환.
inline float Remap(float v, float inLo, float inHi, float outLo, float outHi) {
    return Lerp(outLo, outHi, InverseLerp(inLo, inHi, v));
}

// -----------------------------------------------------------------------------
// 각도 정규화
// -----------------------------------------------------------------------------

// WrapAngle — 라디안 각도를 [-π, π] 범위로 정규화.
//   [8. Numerical Analysis 1.pdf p.5]
//   - fmod 로 [-2π, 2π] 범위로 줄인 뒤 [-π, π] 경계 보정.
//   - 박격포 조준 각도, 방향 차 계산에서 각도 점프를 방지.
inline float WrapAngle(float angle) {
    // fmod 는 피제수의 부호를 따르므로 결과가 [-2π, 2π] 안에 든다.
    float a = std::fmod(angle, 2.0f * kPi);
    // [-π, π] 밖이면 2π 만큼 보정.
    if (a >  kPi) a -= 2.0f * kPi;
    if (a < -kPi) a += 2.0f * kPi;
    return a;
}

// -----------------------------------------------------------------------------
// 이징 함수 (Easing Functions) — namespace Ease
//   [8. Numerical Analysis 1.pdf p.10]
//
//   모든 함수: t ∈ [0, 1] → 반환 값 ∈ [0, 1].
//   f(0) == 0, f(1) == 1 보장.
//   HUD 애니메이션, 화면 전환 페이드, 스코어 팝업 등 시각 효과에 사용.
// -----------------------------------------------------------------------------
namespace Ease {

// InQuad — 가속 곡선 (제곱). 천천히 시작해 빠르게 끝남.
//   [8. Numerical Analysis 1.pdf p.10]
//   - 공식: t².
inline float InQuad(float t) {
    return t * t;
}

// OutQuad — 감속 곡선. 빠르게 시작해 천천히 끝남.
//   [8. Numerical Analysis 1.pdf p.10]
//   - 공식: t * (2 - t).
inline float OutQuad(float t) {
    return t * (2.0f - t);
}

// SmoothStep — S자 곡선 (3차 에르미트). 시작과 끝 모두 부드럽게.
//   [8. Numerical Analysis 1.pdf p.10]
//   - 공식: t² * (3 - 2t).
//   - HP 게이지 변화, 라운드 전환 페이드에 적합.
inline float SmoothStep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

// OutBounce — 바운스 감속. 끝에서 공처럼 튕기는 효과.
//   [8. Numerical Analysis 1.pdf p.10]
//   - 4구간 포물선으로 bounce 를 시뮬레이션.
//   - 스코어 숫자 팝업, UI 요소 등장 애니메이션에 사용.
inline float OutBounce(float t) {
    const float n1 = 7.5625f;
    const float d1 = 2.75f;
    if (t < 1.0f / d1) {
        // 1구간: 첫 번째 포물선.
        return n1 * t * t;
    } else if (t < 2.0f / d1) {
        // 2구간: 두 번째 포물선 (중간 튕김).
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
    } else if (t < 2.5f / d1) {
        // 3구간: 세 번째 포물선 (작은 튕김).
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
    } else {
        // 4구간: 네 번째 포물선 (미세 튕김).
        t -= 2.625f / d1;
        return n1 * t * t + 0.984375f;
    }
}

// OutElastic — 탄성 감속. 끝에서 고무줄처럼 진동하며 멈춤.
//   [8. Numerical Analysis 1.pdf p.10]
//   - 공식: sin 진동 * pow 감쇠.
//   - t=0 이면 0, t=1 이면 1 반환 (경계 조건 명시적 처리).
//   - 폭발 텍스트 팝업, 콤보 알림 등 강조 효과에 사용.
inline float OutElastic(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    const float c4 = (2.0f * kPi) / 3.0f;  // 한 주기 = 3 구간.
    return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
}

} // namespace Ease
