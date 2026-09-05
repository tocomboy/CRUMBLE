// =============================================================================
// src/physics/Resolution.h — 두 body 의 충돌 해소 (위치 보정 + 속도 응답).
//
// 목적:
//   - DetectAABB 등이 채운 Contact 정보를 받아 (1) 침투를 풀어주는 위치 보정,
//     (2) 분리 방향 충격량 응답을 한 번에 처리.
//
// 본 단계 (P1) 의 알고리즘:
//   - kSlop = 0.01f: 미세 잔여 침투를 허용해 jitter 감소.
//   - kPercent = 0.8f: 한 step 에 80 % 만 보정 (남은 20 % 는 이후 step 들에서
//     반복적으로 줄어든다). 위치 보정은 step 당 1 회만 적용해야 한다 — penetration
//     이 반복 사이 갱신되지 않으므로 N 회 적용 시 N 배 과보정 (지면/벽 튕김 버그).
//   - separating velocity (vN > 0) 이면 위치만 보정하고 속도 변화 없음.
//   - 충격량 j = -(1+e) * vN / (invMassA + invMassB) — 표준 1D 탄성 응답
//     (사용자 표면-수직 방향).
//
// 함수 분리:
//   - ResolvePosition : 위치 보정만. ResolveAll 이 step 당 1 회 호출.
//   - ResolveVelocity : 속도 응답만. ResolveAll 이 resolutionIters_ 회 호출.
//   - ResolveContact  : 위치→속도 단일 호출 (단위 테스트 호환용 기존 시맨틱).
//
// 출처:
//   - 메인 플랜 §2.6 Resolution 명세.
//   - 일반 게임 물리의 표준 impulse-based collision response (Baumgarte
//     position correction + 1D restitution impulse).
//   - Physics Engine 2 PDF 미도착 (collision constraint / joint 정렬 미수행).
//
// 결정론:
//   - 모든 산식이 IEEE-754 float. 호출 순서 (PhysicsWorld 의 contacts vector
//     순회) 가 일관되어야 같은 입력에 같은 결과.
//   - kSlop / kPercent 변경 시 P4 replay 테스트 해시 변경 가능 → 변경 금지.
// =============================================================================

#pragma once

// 전방 선언 — 본 헤더가 Collision.h / PhysicsBody.h 를 include 하지 않아도
// 되도록. 호출 측이 두 헤더를 모두 들고 있다고 가정.
struct PhysicsBody;
struct Contact;

// 위치 보정만 — step 당 1 회 호출 (ResolveAll). penetration 비례 보정.
void ResolvePosition(const Contact& c, PhysicsBody& a, PhysicsBody& b);

// 속도 응답만 — 다중 contact 수렴을 위해 여러 번 반복 호출 가능 (ResolveAll).
//   분리 중 (vN > 0) 이면 무동작이라 반복 호출에 안전.
void ResolveVelocity(const Contact& c, PhysicsBody& a, PhysicsBody& b);

// 위치 보정 + 속도 응답 한 번에 처리 (위치→속도 순). 단위 테스트 호환용
// 기존 시맨틱 보존 — ResolveAll 은 위 두 함수를 분리 호출하므로 이 함수를
// 쓰지 않는다.
//   - 입력: Contact c (a/b 인덱스 무시, normal/penetration 만 사용),
//          PhysicsBody& a, b (mutable — pos, vel 변경됨).
//   - 호출 안전 조건: c.normal 은 단위 벡터, c.penetration > 0 이라 가정.
void ResolveContact(const Contact& c, PhysicsBody& a, PhysicsBody& b);
