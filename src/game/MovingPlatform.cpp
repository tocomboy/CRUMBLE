// =============================================================================
// src/game/MovingPlatform.cpp — Kinematic 발판 구현.
// =============================================================================

#include "MovingPlatform.h"

#include <algorithm>
#include <cmath>

#include "../physics/PhysicsBody.h"
#include "../physics/PhysicsWorld.h"

MovingPlatform::MovingPlatform(PhysicsWorld& world,
                               Vec2 center, Vec2 half,
                               PlatformAxis axis, float amplitude, float period)
    : world_(world),
      origin_(center),
      axis_(axis),
      amplitude_(amplitude),
      period_(period)
{
    // PhysicsBody 정의 — Kinematic AABB.
    //   - SetMass(0) 로 무한 질량 (역수 = 0). 충돌 응답이 본 body 를 못 민다.
    //   - restitution 0 / friction 0.4 — Player Dynamic body 가 위에 올라간
    //     상태에서 횡 진동 시 마찰로 수평 끌림이 자연스럽게 부여된다 (P6 의
    //     "platform 위 Player 가 같이 움직임" 시각).
    PhysicsBody def;
    def.pos         = center;
    def.shape       = ShapeType::AABB;
    def.half        = half;
    def.SetMass(0.0f);
    def.restitution = 0.0f;
    def.friction    = 0.4f;
    def.type        = BodyType::Kinematic;
    // P8 collision layer wiring: bit 4 (MovingPlatform). mask=~0u → 모든 layer
    // 와 충돌 (player / chunk / projectile / 다른 platform 모두 막음).
    def.collisionLayer = 1u << 4;
    def.collisionMask  = ~0u;
    bodyId_ = world.CreateBody(def);
}

MovingPlatform::~MovingPlatform() {
    // PhysicsWorld 에 등록된 Kinematic body 정리.
    //   - bodyId_ < 0 가드는 보호적 — ctor 가 항상 valid id 를 반환하므로
    //     실제 음수 케이스는 없음. 다만 PhysicsWorld 의 freeList 의미와 명시.
    if (bodyId_ >= 0) {
        world_.DestroyBody(bodyId_);
    }
}

void MovingPlatform::Update(float dt) {
    // phase 진행. period 가 0 이하면 가속 폭주 위험 — 안전 분기.
    if (period_ <= 0.0f) return;
    phase_ += dt / period_;

    // sin(2π × phase) ∈ [-1, 1] × amplitude.
    //   - 사용자가 직접 만든 cos / 직접 sin 보다 표준 std::sin 이 결정론적.
    constexpr float kTwoPi = 6.28318530718f;
    const float offset = std::sin(phase_ * kTwoPi) * amplitude_;

    PhysicsBody* b = world_.GetBody(bodyId_);
    if (!b) return;

    // 새 위치 계산.
    Vec2 newPos = origin_;
    if (axis_ == PlatformAxis::X) newPos.x += offset;
    else                          newPos.y += offset;

    // 변위 / dt = 이번 step 의 평균 속도. 다음 step 의 충돌 resolution 이
    // 본 vel 로 상대 속도를 추정 → Player 가 위에 올라간 상태면 함께 움직임.
    //   - dt 가 비정상적으로 작을 때 0-나누기 회피 (1e-5f 하한).
    //   - 그 외에는 평균 속도가 정확히 정현파 도함수의 forward-difference 근사.
    const Vec2 dPos = newPos - b->pos;
    const float invDt = 1.0f / std::max(dt, 1e-5f);
    b->vel = Vec2{dPos.x * invDt, dPos.y * invDt};
    b->pos = newPos;
}
