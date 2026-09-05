// =============================================================================
// src/physics/PhysicsBody.cpp — PhysicsBody 의 비-인라인 본문.
//
// 현재는 SetMass 한 함수만 본문 정의. ApplyForce / ApplyImpulse 는 헤더에
// inline 으로 두어 누적 호출 비용을 최소화.
// =============================================================================

#include "PhysicsBody.h"

void PhysicsBody::SetMass(float m) {
    // mass 와 invMass 를 항상 함께 갱신.
    // m > 0 일 때만 invMass 를 계산. 그 외 (0 또는 음수) 는 0 으로 처리.
    //   - 0 처리: Static body 는 invMass = 0 으로 두어 Resolution.cpp 의
    //     `corr / invMassSum` 식이 0 으로 나누어지지 않게 함과 동시에
    //     "위치 보정 0" / "속도 변화 0" 효과를 자연스럽게 얻는다.
    //   - 음수 처리: 본질적으로 비물리적 입력이지만 현재는 0 으로 흡수.
    //     디버깅 가속을 원하면 향후 assert 추가 가능.
    mass = m;
    invMass = (m > 0.0f) ? 1.0f / m : 0.0f;
}
