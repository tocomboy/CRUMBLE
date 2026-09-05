// =============================================================================
// src/physics/Resolution.cpp — 충돌 해소 구현 (위치 보정 / 속도 응답 분리).
//
// 알고리즘 출처: 표준 impulse-based 1D 탄성 / 비탄성 응답. 메인 플랜 §2.6 와
//                일반 게임 물리 텍스트의 정수 패턴.
//
// 위치 보정과 속도 응답을 별도 함수로 분리한 이유 (튕김 버그 root-cause fix):
//   ResolveAll 은 다중 contact 안정화를 위해 응답을 resolutionIters_ (기본 4)
//   회 반복한다. 과거에는 ResolveContact 한 함수가 위치 보정 + 속도 응답을
//   묶어 처리했고, ResolveAll 이 그 함수를 4 회 호출했다. 그런데 위치 보정량은
//   Contact 의 penetration 값에 비례하는데, 이 penetration 은 DetectContacts
//   시점에 한 번 측정된 뒤 반복 사이에 갱신되지 않는다. 따라서 같은 침투값으로
//   매 반복 0.8 배씩 밀어내 한 step 에 약 3.2 배 (1 - 0.2^4 누적이 아니라
//   0.8 × 4 선형 합) 과보정 → Dynamic body 가 지면 / 벽에서 과도하게 튕겨
//   나가는 현상이 발생했다.
//
//   해결: 속도 임펄스는 그대로 여러 번 반복하되 (수렴에 도움), 위치 보정은
//   step 당 정확히 1 회만 적용한다. ResolveAll 이 ResolveVelocity 를 iters 회,
//   ResolvePosition 을 1 회 호출하도록 분리했다. ResolveContact (위치→속도
//   단일 호출) 는 단위 테스트 호환을 위해 기존 시맨틱 그대로 보존한다.
// =============================================================================

#include "Resolution.h"
#include "Collision.h"
#include "PhysicsBody.h"
#include "../math/Vec2.h"

#include <algorithm>     // std::max, std::min.

// ----- 위치 보정 (Position Correction) -----
//   침투를 부드럽게 풀어주는 Baumgarte 식 보정. step 당 1 회만 적용해야 한다
//   (penetration 이 반복 사이 갱신되지 않으므로 N 회 적용 시 N 배 과보정).
void ResolvePosition(const Contact& c, PhysicsBody& a, PhysicsBody& b) {
    // 둘 다 무한 질량이면 보정 불가 (0 으로 나누기 방지).
    float invMassSum = a.invMass + b.invMass;
    if (invMassSum <= 0.0f) {
        return;
    }

    // kSlop:    허용 침투. 0.01 픽셀 이하 잔여 침투는 무시해 jitter 방지.
    //           완벽 보정을 시도하면 다음 step 에 또 미세 침투가 생겨 떨림 발생.
    // kPercent: step 당 보정 비율. 80 % 만 보정하고 남은 20 % 는 다음 step 들에
    //           걸쳐 줄어든다 (한 번에 100 % 보정하면 인접 contact 가 서로 밀어
    //           내며 진동할 수 있다).
    constexpr float kSlop    = 0.01f;
    constexpr float kPercent = 0.8f;

    // 실제 보정량.
    //   - (penetration - kSlop) 으로 슬롭 흡수, max(.,0) 으로 음수 방지.
    //   - / invMassSum 으로 질량 비율 분배 준비, * kPercent 로 step 비율 적용.
    float corr = std::max(c.penetration - kSlop, 0.0f) / invMassSum * kPercent;

    // 분배: 가벼운 body (invMass 큰 쪽) 가 더 많이 밀린다.
    //   - a 는 normal 반대 방향 (a → b 가 normal), b 는 normal 방향.
    a.pos -= c.normal * (corr * a.invMass);
    b.pos += c.normal * (corr * b.invMass);
}

// ----- 속도 응답 (Velocity Response) -----
//   분리 방향 충격량. ResolveAll 이 다중 contact 수렴을 위해 여러 번 반복
//   호출해도 안전하다 (이미 분리 중이면 vN > 0 분기로 무동작).
void ResolveVelocity(const Contact& c, PhysicsBody& a, PhysicsBody& b) {
    float invMassSum = a.invMass + b.invMass;
    if (invMassSum <= 0.0f) {
        return;
    }

    // 상대 속도 (b 에서 본 a) 의 normal 방향 성분.
    //   - vN < 0 : 충돌 방향으로 접근 중 → 응답 필요.
    //   - vN > 0 : 이미 분리 중 → 속도는 그대로 둔다 (이 분기를 빼면 분리 중인
    //              body 도 더 밀어내며 비물리적 튕김 — resolve_separating_
    //              velocity_skips 테스트가 검증).
    Vec2 rv = b.vel - a.vel;
    float vN = Vec2::Dot(rv, c.normal);
    if (vN > 0.0f) {
        return;
    }

    // 반발 계수 e: 두 body 의 restitution 중 작은 값 ("더 부드러운 쪽 우선").
    float e = std::min(a.restitution, b.restitution);

    // 충격량 크기 j (스칼라): j = -(1 + e) * vN / invMassSum.
    //   유도: 충돌 후 분리 속도 = -e * vN, 충격량 J = j * normal 을 양 body 에
    //   invMass 비율로 분배.
    float j = -(1.0f + e) * vN / invMassSum;
    Vec2 impulse = c.normal * j;

    // 가벼운 body 가 더 많이 가속. 무한 질량 (invMass=0) 은 변화 없음.
    a.vel -= impulse * a.invMass;
    b.vel += impulse * b.invMass;

    // 마찰 (접선 방향) 은 본 단계에서는 PhysicsWorld 의 수평 dt-비례 감쇠로
    // 단순화. 정밀한 friction impulse 는 필요 시 추가.
}

// ----- 단일 호출 (위치 보정 + 속도 응답) -----
//   단위 테스트 (test_resolution.cpp) 호환용. 위치 보정 후 속도 응답이라는
//   기존 시맨틱을 그대로 유지한다. ResolveAll 은 이 함수를 쓰지 않고 위
//   두 함수를 분리 호출한다.
void ResolveContact(const Contact& c, PhysicsBody& a, PhysicsBody& b) {
    ResolvePosition(c, a, b);
    ResolveVelocity(c, a, b);
}
