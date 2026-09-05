// =============================================================================
// src/physics/DebugDraw.cpp — F1~F3 디버그 오버레이 구현.
// =============================================================================
//
// 출처:
//   - 메인 플랜 P4 Task 4.4 Step 2 의 코드 + 한국어 주석 보강.
//   - 색상 / 화살표 길이 상수는 시각 명료성 우선으로 선택. P10 폴리싱 시 발표
//     시연에 맞춰 조정 가능.
// =============================================================================

#include "DebugDraw.h"

#include "Collision.h"
#include "PhysicsBody.h"
#include "PhysicsWorld.h"
#include "SpatialGrid.h"

#include <algorithm>  // std::min — pair count bar clamp.
#include <cmath>     // std::cos / std::sin — Circle 외곽 그리기.

namespace {

// 두 점 사이 단순 직선 — SDL_RenderDrawLine 의 얇은 wrapper.
// 향후 P10 에서 화살표 머리 (작은 삼각형) 추가 가능.
void DrawArrow(SDL_Renderer* r, int x0, int y0, int x1, int y1) {
    SDL_RenderDrawLine(r, x0, y0, x1, y1);
}

// AABB 외곽선 — SDL_RenderDrawRect 한 번.
void DrawAabbOutline(SDL_Renderer* r, const PhysicsBody& b) {
    SDL_Rect rect = {
        static_cast<int>(b.pos.x - b.half.x),
        static_cast<int>(b.pos.y - b.half.y),
        static_cast<int>(b.half.x * 2.0f),
        static_cast<int>(b.half.y * 2.0f),
    };
    SDL_RenderDrawRect(r, &rect);
}

// Circle 외곽선 — 36 개의 짧은 segment 로 근사.
//   - 정확한 원이 아니지만 디버그 표시 용도라 36 분할이면 충분히 부드럽다.
//   - SDL2 가 native 원 그리기 API 를 제공하지 않아 line strip 으로 대체.
void DrawCircleOutline(SDL_Renderer* r, int cx, int cy, int radius) {
    constexpr int kSegments = 36;
    constexpr float kPi = 3.14159265f;
    for (int i = 0; i < kSegments; ++i) {
        float t0 = (i + 0) * (2.0f * kPi / kSegments);
        float t1 = (i + 1) * (2.0f * kPi / kSegments);
        SDL_RenderDrawLine(r,
            cx + static_cast<int>(radius * std::cos(t0)),
            cy + static_cast<int>(radius * std::sin(t0)),
            cx + static_cast<int>(radius * std::cos(t1)),
            cy + static_cast<int>(radius * std::sin(t1)));
    }
}

}  // namespace

void DrawPhysicsDebug(SDL_Renderer* r, PhysicsWorld& world, const DebugDrawOptions& opts) {
    const int cap = world.BodyCapacity();

    // -- F1: 모든 살아있는 body 의 hitbox 외곽선 --
    if (opts.drawHitboxes) {
        for (int i = 0; i < cap; ++i) {
            PhysicsBody* b = world.GetBody(i);
            if (!b) continue;
            // Body type 별 색상 — 한 눈에 분류 가능하게 채도 다른 3 색.
            switch (b->type) {
                case BodyType::Static:
                    SDL_SetRenderDrawColor(r, 100, 100, 100, 200);
                    break;
                case BodyType::Kinematic:
                    SDL_SetRenderDrawColor(r,  60,  90, 200, 200);
                    break;
                case BodyType::Dynamic:
                    SDL_SetRenderDrawColor(r, 220, 200,  80, 200);
                    break;
            }
            if (b->shape == ShapeType::AABB) {
                DrawAabbOutline(r, *b);
            } else {
                DrawCircleOutline(r,
                    static_cast<int>(b->pos.x),
                    static_cast<int>(b->pos.y),
                    static_cast<int>(b->half.x));
            }
        }
    }

    // -- F2: 직전 step 의 contact normal 들 --
    if (opts.drawNormals) {
        SDL_SetRenderDrawColor(r, 220, 60, 60, 255);
        // LastContacts() 는 PhysicsWorld::Step 직후 부터 다음 Step 까지만 유효.
        // 본 호출은 메인 루프의 fixed-step 외부 (렌더 단계) 라 안전.
        for (const Contact& c : world.LastContacts()) {
            int x0 = static_cast<int>(c.point.x);
            int y0 = static_cast<int>(c.point.y);
            // normal 길이 24px — 짧으면 안 보이고, 길면 화면 어지러움. 24 가
            // 1280×720 화면에서 시각적 균형.
            int x1 = x0 + static_cast<int>(c.normal.x * 24.0f);
            int y1 = y0 + static_cast<int>(c.normal.y * 24.0f);
            DrawArrow(r, x0, y0, x1, y1);
        }
    }

    // -- F3: dynamic body 의 velocity 화살표 (impulse 의 가시 proxy) --
    //   진정한 impulse 시각화는 한 step 안에서 ApplyImpulse 가 누적된 양을
    //   외부로 노출해야 가능 — P10 단계에서 PhysicsBody 에 lastImpulse 같은
    //   필드 추가해 본격 구현. 본 P4 는 vel 의 0.05x 스케일 화살표로 대체.
    if (opts.drawImpulses) {
        SDL_SetRenderDrawColor(r, 60, 220, 60, 255);
        for (int i = 0; i < cap; ++i) {
            PhysicsBody* b = world.GetBody(i);
            if (!b || b->type == BodyType::Static) continue;
            int x0 = static_cast<int>(b->pos.x);
            int y0 = static_cast<int>(b->pos.y);
            int x1 = x0 + static_cast<int>(b->vel.x * 0.05f);
            int y1 = y0 + static_cast<int>(b->vel.y * 0.05f);
            DrawArrow(r, x0, y0, x1, y1);
        }
    }

    // -- F4: SpatialGrid (dynamic) 의 cell 라인 (P7) --
    //   화면 안 영역의 cell 만 그려 너무 많은 line 으로 화면 어지러움 방지.
    //   세로/가로 라인을 옅은 청회색 으로.
    if (opts.drawGrid) {
        const SpatialGrid& g = world.DynamicGrid();
        const float cs   = g.CellSize();
        const Vec2  wmn  = g.WorldMin();
        const int   cols = g.Cols();
        const int   rows = g.Rows();
        SDL_SetRenderDrawColor(r, 80, 80, 110, 120);

        // 화면 (1280x720) 안에 들어오는 cell 만 표시 — wmn 가 음수일 수 있어
        // 시작 cell 인덱스를 클램프.
        const int viewW = 1280, viewH = 720;
        const int cxStart = std::max(0,        static_cast<int>((0.0f - wmn.x) / cs));
        const int cxEnd   = std::min(cols,     static_cast<int>((viewW - wmn.x) / cs) + 1);
        const int cyStart = std::max(0,        static_cast<int>((0.0f - wmn.y) / cs));
        const int cyEnd   = std::min(rows,     static_cast<int>((viewH - wmn.y) / cs) + 1);

        // 세로 라인 — 각 cell column 의 좌측 경계.
        for (int cx = cxStart; cx <= cxEnd; ++cx) {
            int x = static_cast<int>(wmn.x + cx * cs);
            if (x < 0 || x > viewW) continue;
            SDL_RenderDrawLine(r, x, 0, x, viewH);
        }
        // 가로 라인 — 각 cell row 의 상단 경계.
        for (int cy = cyStart; cy <= cyEnd; ++cy) {
            int y = static_cast<int>(wmn.y + cy * cs);
            if (y < 0 || y > viewH) continue;
            SDL_RenderDrawLine(r, 0, y, viewW, y);
        }
    }

    // -- F5: 직전 step 의 contact 수 bar (P7) --
    //   화면 좌상단 (10, 50) 근처에 노란 막대로 contact 수를 표시.
    //   - 1 contact 당 4 px → 100 contact 면 400 px 막대.
    //   - 본 막대가 차오르면 파괴 / 청크 / projectile 폭증으로 broad-phase 부담
    //     이 늘어나는 시각적 신호. 발표 시 "200 body 에서도 60 fps" 시연용.
    if (opts.drawPairs) {
        const int n = static_cast<int>(world.LastContacts().size());
        SDL_Rect bar = {10, 50, std::min(n * 4, 400), 8};
        SDL_SetRenderDrawColor(r, 220, 220, 80, 255);
        SDL_RenderFillRect(r, &bar);
    }

    // -- F6: Dynamic body 의 velocity 방향 화살표 --
    //   F3 (초록, 0.05× 배율) 와 별도 토글로 독립 운용.
    //   폭발 충격 직후 body 속도 방향 분포를 확인하는 데 쓰인다.
    //   배율 0.08f: F3 (0.05f) 보다 약간 크게 — 저속 body 의 화살표도 눈에 띄도록.
    //   파란 계열 (80, 160, 255): F3 초록 / F2 빨강과 색상 충돌 없이 구분 가능.
    if (opts.drawVelocities) {
        SDL_SetRenderDrawColor(r, 80, 160, 255, 255);
        for (int i = 0; i < cap; ++i) {
            PhysicsBody* b = world.GetBody(i);
            // Static / Kinematic 은 vel 이 항상 0 이라 의미 없음 — Dynamic 만 표시.
            if (!b || b->type != BodyType::Dynamic) continue;
            int x0 = static_cast<int>(b->pos.x);
            int y0 = static_cast<int>(b->pos.y);
            // vel * 0.08: vel 1000 px/s → 화살표 80 px.
            // F3 의 0.05× 보다 약간 길어 동시에 켰을 때 두 화살표가 겹쳐 보이지 않음.
            int x1 = x0 + static_cast<int>(b->vel.x * 0.08f);
            int y1 = y0 + static_cast<int>(b->vel.y * 0.08f);
            DrawArrow(r, x0, y0, x1, y1);
        }
    }
}
