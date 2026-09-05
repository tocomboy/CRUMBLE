// =============================================================================
// src/input/Bindings.cpp — 기본 키 매핑 본문.
//
// 매핑 테이블 (메인 플랜 §4.3):
//   P1            P2
//   ─────────     ──────────
//   A 좌이동      ← 좌이동
//   D 우이동      → 우이동
//   W 점프        ↑ 점프
//   S 하강        ↓ 하강
//   1/2/3 무기    KP 1/2/3 무기
//   F 발사        KP 0 발사
//   G 박격포 각도 KP_ENTER 박격포 각도
// =============================================================================

#include "Bindings.h"
#include "InputManager.h"

void BindDefaultActions(InputManager& im) {
    // ----- P1 (WASD + 일반 숫자행 + F + G) -----
    // WASD: 일반적 PC FPS 관습. scancode 라 AZERTY 사용자도 같은 물리
    // 위치 (좌측 손) 를 사용.
    im.BindAction("p1_move_left",     SDL_SCANCODE_A);
    im.BindAction("p1_move_right",    SDL_SCANCODE_D);
    im.BindAction("p1_jump",          SDL_SCANCODE_W);
    im.BindAction("p1_down",          SDL_SCANCODE_S);

    // 무기 슬롯 1/2/3 (numbers row).
    im.BindAction("p1_weapon_1",      SDL_SCANCODE_1);
    im.BindAction("p1_weapon_2",      SDL_SCANCODE_2);
    im.BindAction("p1_weapon_3",      SDL_SCANCODE_3);

    // 발사 / 박격포 각도 키.
    //   - F: WASD 손 가까이 위치한 발사 키 (왼손 검지).
    //   - G: 박격포 조준 시 각도 토글 (P5 에서 의미 부여).
    im.BindAction("p1_fire",          SDL_SCANCODE_F);
    im.BindAction("p1_mortar_angle",  SDL_SCANCODE_G);

    // ----- P2 (화살표 + 키패드) -----
    // 화살표는 일반적 2P 키맵. 키패드 (Numpad) 는 오른손 사용자 영역.
    im.BindAction("p2_move_left",     SDL_SCANCODE_LEFT);
    im.BindAction("p2_move_right",    SDL_SCANCODE_RIGHT);
    im.BindAction("p2_jump",          SDL_SCANCODE_UP);
    im.BindAction("p2_down",          SDL_SCANCODE_DOWN);

    // 키패드 1/2/3 — 무기 슬롯.
    im.BindAction("p2_weapon_1",      SDL_SCANCODE_KP_1);
    im.BindAction("p2_weapon_2",      SDL_SCANCODE_KP_2);
    im.BindAction("p2_weapon_3",      SDL_SCANCODE_KP_3);

    // 키패드 0 / Enter — 발사 / 박격포 각도.
    im.BindAction("p2_fire",          SDL_SCANCODE_KP_0);
    im.BindAction("p2_mortar_angle",  SDL_SCANCODE_KP_ENTER);
}
