// =============================================================================
// src/game/PlayerStats.h — 플레이어의 게임 상태 (HP / 라운드 점수 / 스폰).
//
// 목적:
//   - PhysicsBody 가 다루지 않는 게임 측 상태 (HP, 죽음 여부, 라운드 승수,
//     리스폰 위치) 를 한 곳에 모은다.
//   - Player 클래스가 멤버로 보유.
//
// 본 단계 (P2) 의 사용 범위:
//   - Respawn() 에서 spawnPoint 사용.
//   - hp / roundsWon / alive 는 P3 의 무기 / 라운드 흐름에서 본격 활용.
//
// 출처: 메인 플랜 §3.4 HP / 데미지 / 낙사 명세.
// =============================================================================

#pragma once

#include "../math/Vec2.h"

struct PlayerStats {
    // ----- HP -----
    // 게임 시작 / 라운드 시작 시 maxHp = 100. 데미지가 hp 를 줄이고,
    // 라운드 종료 시 max 로 복구.
    int  hp    = 100;
    int  maxHp = 100;

    // ----- 라운드 점수 -----
    // 5 라운드 best-of-3. roundsWon 이 3 에 도달하면 매치 승리 (P3 명세).
    int  roundsWon = 0;

    // ----- 스폰 위치 -----
    // Respawn 시 PhysicsBody.pos 를 이 좌표로 강제. 라운드 시작 시 맵의
    // 스폰 포인트로 갱신 (P9 맵 로딩에서 본격 사용).
    Vec2 spawnPoint{0, 0};

    // ----- 피격 플래시 타이머 -----
    // 피격 시 0.15 초 동안 흰색으로 깜빡임 (P10-B Task 6).
    // main.cpp 의 fixed-step 에서 kFixedDt 씩 감산. > 0 이면 플레이어를
    // 정상 색 대신 흰색 (255,255,255) 으로 렌더링.
    float hitFlashTimer = 0.0f;

    // ----- 생존 플래그 -----
    // hp == 0 시 false. RoundEnd 트리거 (P3 GameFlow 의 분기 조건).
    bool alive = true;
};
