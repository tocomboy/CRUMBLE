// =============================================================================
// src/physics/PhysicsWorld.h — 물리 시뮬레이션 월드.
//
// 목적:
//   - PhysicsBody 들을 소유하고, Step(dt) 한 번에 적분 → 충돌 검출 → 응답을
//     수행한다. 게임 측은 CreateBody / GetBody / Step / SetGravity 만 알면 됨.
//
// 본 단계 (P1) 의 알고리즘:
//   - 적분: semi-implicit Euler (impulse 적용 → force 적용 → 속도 갱신 →
//           위치 갱신).
//   - 광역 단계: O(N²) 페어 순회 (P7 에서 SpatialGrid 로 교체).
//   - 응답: ResolveAll 가 속도 임펄스 (ResolveVelocity) 를 resolutionIters_ 회
//           (기본 4) 반복하고, 위치 보정 (ResolvePosition) 은 step 당 1 회만
//           적용 (위치 보정을 반복하면 stale penetration 으로 과보정 → 튕김).
//
// 출처:
//   - 메인 플랜 §2.2 / §2.3 PhysicsWorld API + Step 흐름.
//   - [7. Simulation.pdf] — 일반 충돌 검출 / 응답 패턴.
//   - [9. Numerical Analysis 2.pdf p.3] — Symplectic Euler (semi-implicit Euler)
//     적분기. 본 코드의 "impulse 먼저 적용 → force*dt 적분 → 위치 갱신" 순서가
//     이 페이지의 최소 기준선 패턴과 일치함 (RK4 교체 불필요).
//   - [10. Physics Engine 1.pdf p.6-8] — Broad Phase 개요 + Spatial Hash Grid
//     설계 (DetectContacts 의 dynamicGrid_ / staticGrid_ 분리 구조).
//   - [10. Physics Engine 1.pdf p.11] — DetectCollisions 파이프라인 (broad →
//     layer/mask gate → narrow 순서) — 본 DetectContacts 의 구조와 대응.
//
// 결정론 / 스레드 (비협상 불변식):
//   - Step 은 메인 스레드 단독 호출.
//   - 동일 입력 (body 상태 + dt) → 동일 출력 보장. P4 의 replay 테스트가
//     검증.
//   - 어떤 worker thread 도 bodies_ 에 접근 금지.
//
// Body 풀 관리:
//   - bodies_ 는 vector. id 는 vector 인덱스.
//   - DestroyBody 는 alive_ 플래그를 false 로 두고 freeList_ 에 id 추가.
//     실제 메모리 회수 없이 다음 CreateBody 가 재사용 → id 재사용 가능 (게임
//     로직이 stale id 를 보유하지 않도록 주의해야 한다).
// =============================================================================

#pragma once

#include <functional>
#include <utility>
#include <vector>

#include "../math/Vec2.h"
#include "Collision.h"
#include "PhysicsBody.h"
#include "SpatialGrid.h"

class PhysicsWorld {
public:
    using ContactCallback = std::function<void(const Contact&)>;

    // ----- Body 관리 -----
    // CreateBody: def 의 사본을 풀에 넣고 BodyId 반환.
    //   - freeList_ 가 비어 있지 않으면 해당 id 재사용.
    //   - 그렇지 않으면 vector 끝에 push 하고 그 인덱스 반환.
    BodyId CreateBody(const PhysicsBody& def);

    // DestroyBody: 해당 id 를 dead 표시 + freeList 에 push. 실제 데이터는
    // 다음 CreateBody 가 덮어쓸 때까지 vector 에 남아 있다.
    void   DestroyBody(BodyId id);

    // GetBody: 살아 있는 body 의 포인터, 그 외 nullptr.
    //   - 호출자는 nullptr 체크 필수.
    //   - 반환된 포인터는 다음 CreateBody / DestroyBody 호출 직후 무효화될
    //     수 있음 (vector 재할당 시) — 동일 frame 안에서만 안전하게 사용.
    PhysicsBody* GetBody(BodyId id);

    // ----- 시뮬레이션 -----
    // Step: 한 번의 fixed-timestep 시뮬레이션.
    //   - dt: 초 단위. 메인 루프의 kFixedDt (1/60) 를 그대로 전달.
    //   - 호출 안전: dt > 0, dt <= 0.05 권장 (큰 dt 는 tunneling 위험).
    void Step(float dt);

    // 중력 — 기본 (0, 980) 픽셀/초². 메인 플랜 §3.1 권장 값.
    void SetGravity(Vec2 g) { gravity_ = g; }
    Vec2 GetGravity() const { return gravity_; }

    // ----- Contact 콜백 -----
    // 한 step 동안 발생한 모든 Contact 에 대해 step 끝부분에서 호출.
    // 게임 측 (예: 무기 데미지 처리, 사운드 트리거) 이 등록.
    void SetContactBeginCallback(ContactCallback cb) {
        onContactBegin_ = std::move(cb);
    }

    // ----- 응답 반복 횟수 -----
    int  ResolutionIterations() const { return resolutionIters_; }
    void SetResolutionIterations(int n) { resolutionIters_ = n; }

    // ----- 통계 -----
    int  BodyCount() const;

    // P10 디버그 패널 (F7) 이 풀 크기 확인용.
    int  BodyCapacity() const { return static_cast<int>(bodies_.size()); }

    // P4 의 F2 (contact normal 화살표) 가 직전 step 의 contact 들을 그릴 때
    // 사용. 호출 시점은 Step() 직후 — 다음 Step 시작 시 clear 되므로 그 사이
    // 에만 유효한 read-only view.
    const std::vector<Contact>& LastContacts() const { return contactsThisStep_; }

    // ----- P7: SpatialGrid broad-phase -----
    // MarkStaticDirty: 정적 body 가 추가/삭제/이동될 때 호출. 다음 Step 의
    // DetectContacts 가 staticGrid_ 를 다시 build 하도록 표시.
    //   - P3 의 LoadMap 시 호출. P8 의 TileGrid 파괴 시에도 호출.
    void MarkStaticDirty() { staticGridDirty_ = true; }

    // DynamicGrid: F4 디버그 오버레이 (cell 라인) 가 사용. read-only view.
    const SpatialGrid& DynamicGrid() const { return dynamicGrid_; }
    const SpatialGrid& StaticGrid()  const { return staticGrid_; }

private:
    // 풀: bodies_ 와 alive_ 의 인덱스가 1대1 대응.
    std::vector<PhysicsBody> bodies_;
    std::vector<bool>        alive_;
    std::vector<int>         freeList_;

    // 이번 Step 에서 발견된 contact 들. ResolveAll 와 콜백 분배에 사용.
    std::vector<Contact>     contactsThisStep_;

    Vec2                     gravity_{0.0f, 980.0f};
    ContactCallback          onContactBegin_;

    // 응답 iters: contact 리스트를 같은 횟수만큼 다시 돌며 ResolveContact
    // 호출. 다중 body 사슬 (5 박스 stacking 등) 의 누적 침투를 풀어준다.
    int                      resolutionIters_ = 4;

    // ----- P7: broad-phase grids -----
    // dynamicGrid_:
    //   - 매 step Clear + Insert (모든 dynamic / kinematic body).
    //   - cell 32 px — Player 32x48 / Projectile 6x6 / 발판 120x16 등 평균 본
    //     게임의 body 크기와 잘 맞음.
    //   - 월드 경계는 화면 (1280x720) 보다 크게 잡아 Player 가 화면 밖으로
    //     떨어져도 cell 인덱스 안전.
    SpatialGrid dynamicGrid_{32.0f, Vec2{-2000.0f, -2000.0f}, Vec2{4000.0f, 4000.0f}};
    // staticGrid_:
    //   - 정적 지형 (floor / wall / cover) 은 라운드 동안 변경이 적음.
    //   - staticGridDirty_ flag 가 true 일 때만 rebuild — 매 step 비용 0.
    //   - cell 64 px — 정적 지형은 큰 직사각형이 많아 더 큰 cell 이 효율.
    SpatialGrid staticGrid_{64.0f, Vec2{-2000.0f, -2000.0f}, Vec2{4000.0f, 4000.0f}};
    bool        staticGridDirty_ = true;

    // 내부 단계 함수들 (Step 이 순차 호출).
    void ApplyForcesAndIntegrate(float dt);
    // DetectContacts: SpatialGrid 기반 broad-phase + Narrow 호출 + Layer/Mask
    // 게이트. 결과는 contactsThisStep_ 에 push.
    //   - dynamic vs dynamic : dynamicGrid_.QueryPairs.
    //   - dynamic vs static  : staticGrid_.QueryAABB 로 candidate 추출.
    void DetectContacts();
    void ResolveAll();
};
