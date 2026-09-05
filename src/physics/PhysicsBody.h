// =============================================================================
// src/physics/PhysicsBody.h — 물리 시뮬레이션의 단일 데이터 단위.
//
// 목적:
//   - 메인 플랜 §2.1 의 Static / Kinematic / Dynamic 3 종 body 와 AABB / Circle
//     2 종 shape 를 단일 struct 로 표현. PhysicsWorld 는 PhysicsBody 의
//     vector 를 들고 있고, 충돌 검출 / 응답 / 적분이 모두 이 데이터를 읽고
//     쓴다.
//
// 본 단계 (P1) 의 사용 범위:
//   - AABB 만 사용 (Circle / AABB-Circle 은 P4 에서 활성화).
//   - Static / Dynamic 만 의미 (Kinematic 은 P5 박격포 조준 모드에서 본격
//     사용).
//   - layer / mask 는 정의만 두고 P2 에서 플레이어/투사체 분리에 사용.
//
// 출처:
//   - [7. Simulation.pdf] (36p) — 기본 충돌 검출/응답 패턴 (body 상태 표현,
//     Static/Dynamic 분류).
//   - [10. Physics Engine 1.pdf p.14] — Collision Layer System (collisionLayer /
//     collisionMask 비트 필드 설계 패턴).
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md §2.1 PhysicsBody
//     데이터 명세.
//
// 결정론 / 스레드:
//   - 본 struct 는 단순 데이터. 모든 mutation 은 PhysicsWorld::Step 안에서만,
//     메인 스레드에서만 수행. 외부 코드는 GetBody 로 포인터를 받아 읽기 위주.
//
// 메모리 레이아웃 노트:
//   - 필드 순서는 캐시 친화성보다 가독성 우선. 본 게임 규모 (~200 body) 에선
//     L1 캐시 피팅이 주요 병목이 아님. Performance Budget 6ms sim 예산 안에서
//     단순 vector iteration 으로 충분.
// =============================================================================

#pragma once

#include <cstdint>           // uint8_t / uint32_t — 비트 폭 보장.

#include "../math/Vec2.h"

// BodyType: 시뮬레이션 동작 모드 분류.
//   - Static    : 절대 움직이지 않음. 중력/충격량 영향 없음. invMass = 0.
//                 (예: 맵의 고정 벽, 바닥, 파괴되지 않는 플랫폼)
//   - Kinematic : 외부 힘은 무시하지만 vel 을 직접 설정해 움직일 수 있음.
//                 (예: P6 MovingPlatform 의 sin-wave 운동, P5 MortarAiming
//                  중의 일시 동결된 플레이어 — 메인 플랜 §3.1 참조)
//   - Dynamic   : 중력 + impulse + 충돌 응답 모두 적용. 일반 플레이어 / 투사체
//                 / 청크.
//
// uint8_t 베이스: 8 비트면 충분, 메모리 절약.
enum class BodyType  : uint8_t {
    Static,
    Kinematic,
    Dynamic
};

// ShapeType: 충돌 도형 종류.
//   - AABB   : 축 정렬 바운딩 박스. half = (halfWidth, halfHeight).
//   - Circle : 원. half.x = 반지름. half.y 는 무시.
//
// 본 게임의 모든 충돌 도형이 둘 중 하나로 통일되어 있어 narrow-phase dispatch
// 가 4 가지 (AABB-AABB / AABB-Circle / Circle-AABB / Circle-Circle) 로 단순화.
enum class ShapeType : uint8_t {
    AABB,
    Circle
};

// PhysicsBody — 시뮬레이션의 한 개체.
//   - struct (POD-like). 클래스 캡슐화 없이 직접 필드 접근.
//   - 기본값은 가장 흔한 케이스 (Dynamic, AABB 16x16, mass=1, friction=0.5,
//     restitution=0) 로 두어 호출 측이 필요한 필드만 덮어쓰면 된다.
struct PhysicsBody {
    // ----- 운동학 상태 -----
    Vec2 pos;                       // 월드 좌표 (픽셀 단위, y 아래쪽이 +).
    Vec2 vel;                       // 속도 (픽셀/초).
    // 이번 step 동안 누적될 힘 (force, N 단위 추상화).
    // 매 step 끝에 0 으로 리셋된다 — Step 안의 ApplyForcesAndIntegrate 가 처리.
    Vec2 forceAccum;
    // 이번 step 동안 누적될 충격량 (impulse).
    // Force 와 분리한 이유: impulse 는 즉시 속도 변화를 만들고 (충돌 응답,
    // 폭발 충격), force 는 dt 적분 후 누적된다 (중력, drag).
    // 메인 플랜 §2.1 의 명시적 분리 규칙.
    Vec2 impulseAccum;

    // ----- 질량 -----
    // mass = 0 인 Static body 는 사실상 "무한 질량".
    // invMass = 1/mass (또는 0). SetMass() 가 이 둘을 함께 갱신한다 — 두
    // 필드가 항상 동기화 되어 있음을 외부 코드가 신뢰할 수 있어야 함.
    float mass    = 1.0f;
    float invMass = 1.0f;

    // ----- 도형 -----
    ShapeType shape = ShapeType::AABB;
    // AABB: half-extents = (halfWidth, halfHeight).
    // Circle: half.x 만 사용 (반지름).
    Vec2  half = {16.0f, 16.0f};

    // ----- 재질 (충돌 응답 파라미터) -----
    // restitution: 반발 계수. 0 = 완전 비탄성 (튕기지 않음), 1 = 완전 탄성.
    //   기본 0.0 = 게임 객체 대부분이 부드럽게 멈춤.
    //   P5 의 박격포 청크는 약간의 튐 (0.2~0.3) 을 주어 시각적 임팩트.
    float restitution = 0.0f;
    // friction: 마찰. 본 P1 단계에서는 수평 속도 dt 비례 감쇠로 단순화.
    //   기본 0.5 = 60Hz 에서 1초당 0.83 배율 정도 감속.
    float friction    = 0.5f;

    // ----- 시뮬레이션 분류 -----
    BodyType type = BodyType::Dynamic;

    // ----- 충돌 layer / mask -----
    // collisionLayer: 자신이 속한 레이어 비트 (multi-bit OR 가능).
    //   (예: bit 0 = Default, bit 1 = Player, bit 2 = Projectile, bit 3 = Tile,
    //        bit 4 = Chunk)
    // collisionMask:  자신이 충돌해야 하는 상대 레이어 비트들의 OR.
    //   (예: Player 의 mask 는 Default|Tile|Chunk 비트들의 OR)
    //
    // 충돌 검출 시: 둘 중 하나라도 (a.mask & b.layer == 0) 이면 스킵.
    //
    // 기본값 (collisionLayer=1, collisionMask=~0u) 의미:
    //   - 본 body 는 "default" 레이어 (bit 0) 에 속함.
    //   - 모든 다른 레이어와 충돌.
    //   → 별도 설정 없이 생성한 두 body 는 자동으로 서로 충돌 (a.mask & b.layer
    //     = ~0u & 1 = 1 != 0).
    //
    // 플랜 §2.1 원본 명세는 collisionLayer=0 이었으나 이 경우 모든 mask 와의
    // AND 가 0 이 되어 "어떤 충돌도 발생 안 함" 함정. 본 프로젝트는 P0 빌드
    // 검증 시 발견된 결함을 root-cause 수정으로 default=1 로 변경했다.
    //   (메인 플랜 §2.1 의 본 기본값도 향후 동기화 필요 — Plan Revisability
    //    Policy 의 코드 정합 fix 카테고리.)
    //
    // 의도적 분리가 필요한 케이스 (P5 같은 팀 투사체끼리 안 부딪히게 등) 에서
    // 호출자가 layer / mask 를 명시 설정.
    uint32_t collisionLayer = 1;
    uint32_t collisionMask  = ~0u;

    // userData: 게임 측에서 본 body 가 무엇인지 식별할 수 있게 두는 핸들.
    //   (예: Player* 또는 Projectile* 를 캐스팅해 저장)
    //   void* 인 이유: 물리 코어가 게임 타입을 알 필요가 없도록 의존성 차단.
    void* userData = nullptr;

    // ----- 메서드 -----
    // SetMass: mass 와 invMass 를 일관되게 설정.
    //   - m > 0  → mass = m, invMass = 1/m.
    //   - m == 0 → mass = 0, invMass = 0 (Static / 무한 질량).
    //   - m  < 0 은 호출자 책임 (현재 0 처리, 디버깅 시 빠른 발견을 위해
    //     명시적 assert 는 추가 안 함).
    void SetMass(float m);

    // 인라인 누적 — force/impulse 는 단순 += 로 충분.
    void ApplyForce(Vec2 f)   { forceAccum   += f; }
    void ApplyImpulse(Vec2 j) { impulseAccum += j; }
};
