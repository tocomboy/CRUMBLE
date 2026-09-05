// =============================================================================
// src/render/Text.cpp — TTF 텍스트 렌더 구현.
// =============================================================================

#include "Text.h"

void DrawText(SDL_Renderer* r, TTF_Font* font, const std::string& text,
              int x, int y, SDL_Color color, bool centered) {
    if (!font || text.empty() || !r) return;

    // 1) Surface 생성 — Blended 는 alpha 가 부드럽게 섞이는 anti-aliased.
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surf) return;

    // 2) Texture 변환 — GPU 로 업로드. 한 줄 렌더 후 즉시 해제 (캐시 안 함).
    //    문자 수가 많거나 매 frame 호출되면 P10 폴리싱에서 SDL_Texture 캐시
    //    검토 가능 (Title / Controls 같은 정적 화면은 매 frame 동일 텍스트
    //    이므로 캐시 효과적).
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return;
    }

    // 3) 목적지 사각형 — centered 면 surface 크기의 절반만큼 좌상단을 보정.
    SDL_Rect dst{x, y, surf->w, surf->h};
    if (centered) {
        dst.x = x - surf->w / 2;
        dst.y = y - surf->h / 2;
    }
    SDL_RenderCopy(r, tex, nullptr, &dst);

    // 4) 자원 정리.
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

int TextWidth(TTF_Font* font, const std::string& text) {
    if (!font || text.empty()) return 0;
    int w = 0, h = 0;
    TTF_SizeUTF8(font, text.c_str(), &w, &h);
    return w;
}
