// =============================================================================
// src/physics/Collision.h — narrow-phase 충돌 검출 + Contact 데이터.
//
// 목적:
//   - 두 PhysicsBody 가 실제로 겹치는지 판단하고, 겹친다면 응답에 필요한
//     정보 (normal, penetration, point, relativeVel) 를 Contact 구조체로
//     채워준다.
//
// 본 단계 (P1) 의 사용 범위:
//   - DetectAABB 본 구현. 다른 함수는 P4 에서 활성화하기 전 stub (return false).
//   - Narrow dispatch 는 AABB-AABB 만 라우팅.
//
// 출처:
//   - [7. Simulation.pdf] — 기본 AABB 충돌 검출 패턴 (overlap-min-axis normal
//     결정).
//   - [10. Physics Engine 1.pdf p.11] — DetectCollisions 파이프라인에서
//     narrow-phase 가 broad-phase 후보를 받아 정밀 검사하는 구조.
//   - 메인 플랜 §2.5 Narrow-phase 명세.
//
// 결정론:
//   - 모든 비교가 IEEE-754 float. 호출 순서 (i < j) 가 일관되어야 동일 입력
//     에 동일 결과. PhysicsWorld::DetectContacts 가 정렬된 페어 순서로 본
//     함수를 호출해 순서 의존성을 차단한다.
// =============================================================================

#pragma once

#include "../math/Vec2.h"

// 전방 선언 — Collision.h 가 PhysicsBody.h 를 include 하지 않아도 되도록.
// PhysicsBody* 만 다루면 헤더 의존이 줄어들어 컴파일 시간이 빨라진다.
struct PhysicsBody;

// BodyId: 본 게임에서는 PhysicsWorld 의 vector 인덱스로 일원화.
//   - int 로 선언해 음수 (== 무효 / 미설정) 표현 가능.
//   - using 별칭 형태로 두면 향후 강한 타입 (struct BodyId) 으로 바꾸기 쉬움.
using BodyId = int;

// Contact: 충돌 한 쌍의 응답 정보.
//   - a, b      : PhysicsWorld 가 채우는 BodyId 인덱스 (Detect* 는 -1 그대로
//                 둠 — 호출자 책임).
//   - normal    : a → b 방향 단위 벡터. Resolution 이 위치 보정에 사용.
//   - penetration: 침투 깊이 (>0). 슬롭 / kPercent 와 함께 위치 보정량 계산.
//   - point     : 충돌 추정 지점 (현재는 두 중심의 중점 — P4 에서 정밀화 가능).
//   - relativeVel: b.vel - a.vel. Resolution 의 separating velocity 검사에 사용.
struct Contact {
    BodyId a = -1;
    BodyId b = -1;

    // 기본 normal 을 (1, 0) 으로 둔 이유: 0 벡터로 두면 Normalized 호출 시
    // (0, 0) 이 되어 위치 보정량이 0 으로 처리되는 함정이 있어 시각적 fallback.
    Vec2  normal{1.0f, 0.0f};

    float penetration = 0.0f;
    Vec2  point{};
    Vec2  relativeVel{};
};

// AABB-AABB: 두 직사각형이 겹치는지 판정.
//   - 반환: 겹치면 true 와 out 채움. 안 겹치면 false (out 미정의).
//   - 정확한 알고리즘은 .cpp 에 (overlap-min-axis 패턴).
bool DetectAABB(const PhysicsBody& a, const PhysicsBody& b, Contact& out);

// Circle-Circle 검출 — P4 에서 활성화. 본 단계에서는 stub return false.
bool DetectCircle(const PhysicsBody& a, const PhysicsBody& b, Contact& out);

// AABB-Circle 혼합 검출 — P4 에서 활성화. stub.
bool DetectAABBCircle(const PhysicsBody& aabb, const PhysicsBody& circle, Contact& out);

// Narrow: 두 body 의 shape 조합에 따라 적절한 Detect* 로 dispatch.
//   - P1: AABB-AABB 만 라우팅. Circle 관련은 false.
//   - P4 에서 4 가지 조합 모두 라우팅 활성화.
bool Narrow(const PhysicsBody& a, const PhysicsBody& b, Contact& out);
