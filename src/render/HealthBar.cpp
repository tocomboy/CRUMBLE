// =============================================================================
// src/render/HealthBar.cpp — 대칭 HP 바 SDL 렌더링.
// =============================================================================
//
// 출처:
//   - 메인 플랜 P3 Task 3.5 Step 2 의 코드 + 한국어 주석 보강.
//   - SDL_Rect / SDL_RenderFillRect API 는 [4. Rendering.pdf] 패턴.
//   - P10-B Task 5: LerpColor 2단계 그라디언트 (빨강→노랑→초록).
//     [8. Numerical Analysis 1.pdf p.2] Lerp 원리 + RenderUtils.h 래퍼 사용.
// =============================================================================

#include "HealthBar.h"
#include "../math/MathUtils.h"  // Lerp — 스칼라 보간.
#include "RenderUtils.h"        // LerpColor — SDL_Color 채널별 보간.

#include <algorithm>   // std::max / std::min — ratio 클램프.

void DrawSplitHpBar(SDL_Renderer* r, int screenW,
                    float p1Ratio, float p2Ratio) {
    // 레이아웃 상수 — 본 함수 내부에서만 사용. 본 P3 단계는 UI 파라미터를
    // 한 곳에 모아두지 않고 함수별 constexpr 로 둠. P10 폴리싱에서 UiTokens
    // 로 통일 가능.
    constexpr int kBarH = 24;   // 막대 높이.
    constexpr int kBarY = 16;   // 화면 상단에서의 y 오프셋.
    constexpr int kGap  = 80;   // 가운데 비우는 폭 (라운드 dot 영역).

    // 각 측이 차지하는 가로 폭 — kGap 을 빼고 반.
    int half = (screenW - kGap) / 2;

    // ratio 클램프 — 호출자가 음수/1초과 값을 줘도 안전.
    p1Ratio = std::max(0.0f, std::min(1.0f, p1Ratio));
    p2Ratio = std::max(0.0f, std::min(1.0f, p2Ratio));

    // 길이 계산:
    //   p1FillW : 0~half 사이. ratio=1 이면 half (꽉 참).
    //   p1FillX : 좌측 끝점. 막대가 "오른쪽으로 자란다" — half-fill 만큼
    //             오른쪽으로 이동. 즉 막대의 우측 끝이 항상 중앙 (=half) 에
    //             고정. HP=full 이면 좌측 0 부터 시작, HP=0 이면 좌측 half
    //             지점 (= 보이지 않음).
    int p1FillW = static_cast<int>(half * p1Ratio);
    int p1FillX = half - p1FillW;
    // p2 는 좌측 끝이 (half + kGap) 에서 시작. 가장 단순한 좌→우 막대.
    int p2Left  = half + kGap;
    int p2FillW = static_cast<int>(half * p2Ratio);

    // 4 개의 SDL_Rect — P1 배경 / P1 채움 / P2 배경 / P2 채움.
    SDL_Rect p1Bg   = {0,        kBarY, half,    kBarH};
    SDL_Rect p1Fill = {p1FillX,  kBarY, p1FillW, kBarH};
    SDL_Rect p2Bg   = {p2Left,   kBarY, half,    kBarH};
    SDL_Rect p2Fill = {p2Left,   kBarY, p2FillW, kBarH};

    // 색상 그라디언트 — P10-B Task 5.
    //   HP 비율에 따라 2단계 LerpColor:
    //     ratio < 0.5 : 빨강(220,30,30) → 노랑(220,200,30)
    //     ratio >= 0.5: 노랑(220,200,30) → 초록(30,200,30)
    //   [8. Numerical Analysis 1.pdf p.2] — Lerp 원리 적용.
    //   배경은 양쪽 모두 회색 (40,40,40) — 플레이어 구분색 대신 중립 회색으로
    //   통일해 그라디언트 색이 도드라지도록.
    auto hpGradient = [](float ratio) -> SDL_Color {
        // ratio 를 [0,1] 클램프 (호출 전 이미 클램프됐지만 안전 재처리).
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        const SDL_Color kRed    = {220,  30,  30, 255};
        const SDL_Color kYellow = {220, 200,  30, 255};
        const SDL_Color kGreen  = { 30, 200,  30, 255};
        if (ratio < 0.5f) {
            // 0 ~ 0.5 → 빨강 → 노랑 : t = ratio / 0.5.
            return LerpColor(kRed, kYellow, ratio / 0.5f);
        } else {
            // 0.5 ~ 1.0 → 노랑 → 초록 : t = (ratio - 0.5) / 0.5.
            return LerpColor(kYellow, kGreen, (ratio - 0.5f) / 0.5f);
        }
    };

    const SDL_Color bgColor  = {40, 40, 40, 255};  // 중립 회색 배경.
    const SDL_Color p1Color  = hpGradient(p1Ratio);
    const SDL_Color p2Color  = hpGradient(p2Ratio);

    SDL_SetRenderDrawColor(r, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    SDL_RenderFillRect(r, &p1Bg);
    SDL_SetRenderDrawColor(r, p1Color.r, p1Color.g, p1Color.b, p1Color.a);
    SDL_RenderFillRect(r, &p1Fill);
    SDL_SetRenderDrawColor(r, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    SDL_RenderFillRect(r, &p2Bg);
    SDL_SetRenderDrawColor(r, p2Color.r, p2Color.g, p2Color.b, p2Color.a);
    SDL_RenderFillRect(r, &p2Fill);

    // 흰 외곽선 — 어두운 배경에서도 막대 영역의 경계를 명확히.
    SDL_SetRenderDrawColor(r, 220, 220, 220, 255);
    SDL_RenderDrawRect(r, &p1Bg);
    SDL_RenderDrawRect(r, &p2Bg);
}
