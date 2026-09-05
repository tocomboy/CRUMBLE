// =============================================================================
// src/game/MovingPlatform.h — Kinematic 발판 (정현파 진동).
//
// 목적:
//   - PhysicsWorld 안에 BodyType::Kinematic body 한 개 (AABB) 를 갖고, 매
//     fixed-step 마다 정현파 함수로 위치를 갱신.
//   - 적분기는 Kinematic body 의 vel/forceAccum 을 무시하므로, 본 클래스가
//     pos 를 직접 set + vel 도 함께 (변위 / dt) 로 set 해 충돌 응답에서
//     "발판이 player 를 같이 끌고 가는" 효과가 나오게 한다.
//
// 동작 모델:
//   - axis = X 또는 Y 단일 축 진동.
//   - offset(t) = sin(2π * t / period) * amplitude.
//   - newPos = origin + axis_unit * offset(t).
//   - vel = (newPos - oldPos) / dt — 다음 step 의 collision resolution 이
//     상대 속도 차이에 임펄스를 주는 데 본 vel 을 사용.
//
// 결정론:
//   - 매 step dt 가 일정 (kFixedDt=1/60) → phase 누적이 동일 → 같은 결과.
//   - sin 은 GCC/Clang 의 표준 라이브러리 (수학 환경 동일) 라 byte-equal
//     보장. -ffast-math 금지가 본 결정론도 함께 보호.
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 6 Task 6.2.
//   - 메인 스펙 §3.2 — Kinematic body 는 무한 질량 + 외력 무시 + 직접 운동.
//   - [7. Simulation.pdf] — 향후 강의 PDF 가 도착하면 정현파 적분 패턴이
//     PDF 의 운동 모델과 정렬되는지 Plan Revisability Policy 절차로 점검.
// =============================================================================

#pragma once

#include "../math/Vec2.h"

class PhysicsWorld;
struct PhysicsBody;

// PlatformAxis: 진동 축. X 는 좌/우 횡 진동, Y 는 상/하 종 진동.
//   - 두 축을 동시에 합치는 건 본 P6 단계에서 미지원 — 필요 시 후속 phase 에서
//     enum 을 비트마스크로 확장하거나 두 인스턴스를 겹쳐 배치.
enum class PlatformAxis {
    X,
    Y
};

class MovingPlatform {
public:
    // 생성자:
    //   - center      : 진동 중심 (origin). 진동은 이 점을 기준으로 ±amplitude.
    //   - half        : AABB half size (X half, Y half).
    //   - axis        : 진동 축.
    //   - amplitude   : 진동 진폭 (px). offset 의 절댓값 상한.
    //   - period      : 한 주기 시간 (s). 작을수록 빠른 진동.
    MovingPlatform(PhysicsWorld& world,
                   Vec2 center, Vec2 half,
                   PlatformAxis axis, float amplitude, float period);

    // P9 C1 — 소멸자에서 등록된 Kinematic body 를 PhysicsWorld 에서 destroy.
    //   - LoadMap 의 라운드/매치 사이 tear-down (ctx.platforms.clear()) 에서
    //     unique_ptr 소멸자가 본 함수를 호출하여 누수 방지.
    //   - GameContext 의 멤버 destroy 순서 (선언 역순) 가 platforms → world
    //     이라 본 호출 시점에 world_ 는 여전히 살아있음.
    ~MovingPlatform();

    MovingPlatform(const MovingPlatform&)            = delete;
    MovingPlatform& operator=(const MovingPlatform&) = delete;

    // 매 fixed-step 호출 (world.Step 직전).
    //   - phase 를 dt/period 만큼 진행.
    //   - newPos 계산 후 body->pos 갱신 + body->vel = (newPos - oldPos)/dt.
    //   - dt 가 0 또는 매우 작으면 0-나누기 회피 (max 1e-5f).
    void Update(float dt);

    int GetBodyId() const { return bodyId_; }

private:
    PhysicsWorld& world_;
    int           bodyId_;
    Vec2          origin_;        // 진동 중심.
    PlatformAxis  axis_;
    float         amplitude_;
    float         period_;
    // phase_ 단위는 "주기 수" (즉 1.0 이면 한 바퀴). sin 인자는 phase * 2π.
    //   - 누적이 매우 커지면 부동소수 정밀 손실 가능 → 실제로는 60 fps × 임의
    //     라운드 길이로 30분도 ~3600 주기 = 안전. 별도 modulo 안 함.
    float         phase_ = 0.0f;
};
