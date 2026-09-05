// =============================================================================
// src/render/HUD.cpp — HUD 합성 (HP 바 + dot 점수).
// =============================================================================
//
// 출처:
//   - 메인 플랜 P3 Task 3.5 Step 4 의 코드 + 한국어 주석 보강.
//   - 텍스트 부재 정책: P3 단계는 SDL2_ttf 의존 추가 없이 dot 으로 점수만
//     표현. P9 에서 ResourceManager + 폰트 도입 후 텍스트화.
// =============================================================================

#include "HUD.h"

#include "HealthBar.h"

void DrawHUD(SDL_Renderer* r, int screenW,
             const Player& p1, const Player& p2,
             int p1RoundsWon, int p2RoundsWon) {
    // -- 1) HP 바 -- ----------------------------------------------------------
    // 0 으로 나누지 않도록 maxHp 가 0 인 비정상 케이스만 1 로 보정. 정상 흐름
    // 에서는 PlayerStats.maxHp = 100.
    auto safeRatio = [](int hp, int maxHp) {
        if (maxHp <= 0) return 0.0f;
        return static_cast<float>(hp) / static_cast<float>(maxHp);
    };
    float p1Ratio = safeRatio(p1.Stats().hp, p1.Stats().maxHp);
    float p2Ratio = safeRatio(p2.Stats().hp, p2.Stats().maxHp);
    DrawSplitHpBar(r, screenW, p1Ratio, p2Ratio);

    // -- 2) 라운드 점수 dot -- ------------------------------------------------
    // 화면 정중앙 (cx) 을 기준으로:
    //   P1 측 dot 3 개: cx-30, cx-10, cx+10 (좌측에서 가까운 순으로 i=0,1,2)
    //   P2 측 dot 3 개: cx+30, cx+10, cx-10 (우측에서 가까운 순으로 i=0,1,2)
    // 채워진 dot = 승리한 라운드 (각자의 색), 빈 dot = 외곽선만.
    constexpr int kBarY = 16;
    constexpr int kBarH = 24;
    const     int cx   = screenW / 2;
    constexpr int dotR = 4;

    for (int i = 0; i < 3; ++i) {
        // -- P1 dot -- (왼쪽으로부터 i 번째)
        int x = cx - 30 + i * 20;
        SDL_Rect d = {x - dotR, kBarY + kBarH / 2 - dotR, dotR * 2, dotR * 2};
        if (i < p1RoundsWon) {
            // 승리 — 빨강 채움.
            SDL_SetRenderDrawColor(r, 220, 30, 30, 255);
            SDL_RenderFillRect(r, &d);
        } else {
            // 미승리 — 흰 외곽선만.
            SDL_SetRenderDrawColor(r, 220, 220, 220, 255);
            SDL_RenderDrawRect(r, &d);
        }

        // -- P2 dot -- (오른쪽으로부터 i 번째)
        int x2 = cx + 30 - i * 20;
        SDL_Rect d2 = {x2 - dotR, kBarY + kBarH / 2 - dotR, dotR * 2, dotR * 2};
        if (i < p2RoundsWon) {
            SDL_SetRenderDrawColor(r, 30, 80, 220, 255);
            SDL_RenderFillRect(r, &d2);
        } else {
            SDL_SetRenderDrawColor(r, 220, 220, 220, 255);
            SDL_RenderDrawRect(r, &d2);
        }
    }
}
