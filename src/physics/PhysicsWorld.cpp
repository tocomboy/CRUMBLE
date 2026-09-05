// =============================================================================
// src/physics/PhysicsWorld.cpp — Step 오케스트레이션 + body pool 관리.
// =============================================================================

#include "PhysicsWorld.h"
#include "Resolution.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

// ----- Body 관리 -----

BodyId PhysicsWorld::CreateBody(const PhysicsBody& def) {
    // freeList_ 에 빈 슬롯이 있으면 재사용 (id 재사용).
    if (!freeList_.empty()) {
        int id = freeList_.back();
        freeList_.pop_back();
        bodies_[id] = def;          // 새 데이터 덮어쓰기.
        alive_[id]  = true;
        return id;
    }
    // 빈 슬롯 없음 → 끝에 push.
    bodies_.push_back(def);
    alive_.push_back(true);
    return static_cast<int>(bodies_.size()) - 1;
}

void PhysicsWorld::DestroyBody(BodyId id) {
    // 범위 체크.
    if (id < 0 || id >= static_cast<int>(bodies_.size())) {
        return;
    }
    // 이미 dead 면 무동작 (다중 destroy 안전).
    if (!alive_[id]) {
        return;
    }
    alive_[id] = false;
    freeList_.push_back(id);
    // bodies_[id] 의 데이터는 그대로 두어 도시락처럼 다음 CreateBody 의
    // 덮어쓰기 비용을 줄인다.
}

PhysicsBody* PhysicsWorld::GetBody(BodyId id) {
    if (id < 0 || id >= static_cast<int>(bodies_.size())) {
        return nullptr;
    }
    if (!alive_[id]) {
        return nullptr;
    }
    return &bodies_[id];
}

int PhysicsWorld::BodyCount() const {
    int n = 0;
    for (bool a : alive_) {
        if (a) ++n;
    }
    return n;
}

// ----- 적분 -----

void PhysicsWorld::ApplyForcesAndIntegrate(float dt) {
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        if (!alive_[i]) continue;

        PhysicsBody& b = bodies_[i];

        // Static: 이번 step 의 force / impulse / 적분 모두 무시.
        // (force/impulse 누적값은 그대로 두지만 Static 은 외부 코드가
        //  Apply 호출하지 않는다는 전제. 만일 호출되더라도 영향 없음.)
        if (b.type == BodyType::Static) {
            continue;
        }

        // 1) Impulse 즉시 적용 — 속도 변화 (단위: m/s, 픽셀/s).
        //    충돌 응답 / 폭발 / 외부 큐 (P5 박격포 폭발) 가 누적해 둔 값.
        b.vel += b.impulseAccum * b.invMass;
        b.impulseAccum = Vec2{};   // 다음 step 을 위해 0 으로 리셋.

        if (b.type == BodyType::Dynamic) {
            // 2) Gravity 와 forceAccum 결합.
            //    - gravity 는 Newton 의 F=ma 에 따라 mass 를 곱해 force 형태로
            //      누적 (a * mass = F). 다음 줄에서 F * invMass * dt 가 dt
            //      간 속도 변화 = a * dt.
            b.forceAccum += gravity_ * b.mass;

            // 3) Force → 속도 적분: v += (F * invMass) * dt
            //    Static (invMass=0) 은 0 누적이므로 안전하지만, 위 분기로
            //    이미 걸러져 여기 안 들어옴.
            b.vel += (b.forceAccum * b.invMass) * dt;
            b.forceAccum = Vec2{};

            // 4) 수평 마찰 (간이): 1초당 friction 비율로 감속.
            //    1 - friction*dt 로 dt 비례 감쇠. friction=0.5, dt=1/60 →
            //    1 - 0.0083 = 0.9917 (1초당 약 0.95 배).
            //    음수 안전: max(0, .) 로 강제.
            b.vel.x *= std::max(0.0f, 1.0f - b.friction * dt);
        }

        // 5) 위치 적분 (Dynamic + Kinematic 둘 다).
        //    Kinematic 은 외부 코드가 vel 을 직접 설정한 뒤 본 줄에서 위치
        //    적분만 가져간다 (예: P6 MovingPlatform 의 sin-wave 운동).
        b.pos += b.vel * dt;
    }
}

// ----- 충돌 검출 (광역 + narrow) -----

void PhysicsWorld::DetectContacts() {
    contactsThisStep_.clear();

    // ----- 정적 grid build (dirty 일 때만) -----
    //   Static body 는 라운드 동안 변경이 적어 매 step rebuild 가 낭비. P3 의
    //   LoadMap / P8 의 TileGrid 파괴 등이 MarkStaticDirty() 를 호출 → 다음
    //   step 의 본 분기가 grid 다시 채움.
    if (staticGridDirty_) {
        staticGrid_.Clear();
        for (std::size_t i = 0; i < bodies_.size(); ++i) {
            if (!alive_[i]) continue;
            const PhysicsBody& b = bodies_[i];
            if (b.type != BodyType::Static) continue;
            // Circle 은 외접 정사각형 AABB 로 등록 (broad-phase 정확도는 narrow
            // 에서 보강. broad 의 false positive 는 성능 비용만 있고 결과 무영향).
            AABB bd{b.pos, b.shape == ShapeType::Circle ? Vec2{b.half.x, b.half.x}
                                                        : b.half};
            staticGrid_.Insert(static_cast<int>(i), bd);
        }
        staticGridDirty_ = false;
    }

    // ----- 동적 grid 매 step rebuild -----
    //   Dynamic / Kinematic 은 매 step 위치가 바뀜 → 매번 새로 build.
    dynamicGrid_.Clear();
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        if (!alive_[i]) continue;
        const PhysicsBody& b = bodies_[i];
        if (b.type == BodyType::Static) continue;
        AABB bd{b.pos, b.shape == ShapeType::Circle ? Vec2{b.half.x, b.half.x}
                                                    : b.half};
        dynamicGrid_.Insert(static_cast<int>(i), bd);
    }

    // 공통 람다 — broad-phase 통과한 (i, j) 페어를 narrow 로 검사 후 contact 추가.
    //   - layer/mask 게이트 + Narrow 호출 + i<j 정규화는 호출자 책임.
    auto check = [&](int i, int j) {
        if (i == j) return;
        const PhysicsBody& a = bodies_[i];
        const PhysicsBody& b = bodies_[j];
        // Layer / mask 게이트 — naive 와 동일.
        if ((a.collisionMask & b.collisionLayer) == 0) return;
        if ((b.collisionMask & a.collisionLayer) == 0) return;
        Contact c;
        if (Narrow(a, b, c)) {
            c.a = i;
            c.b = j;
            contactsThisStep_.push_back(c);
        }
    };

    // ----- Dynamic vs Dynamic -----
    //   pair 는 (i<j) 로 정규화되어 SpatialGrid 가 sort+unique → 결정론.
    std::vector<std::pair<int,int>> dynPairs;
    dynamicGrid_.QueryPairs(dynPairs);
    for (const auto& p : dynPairs) {
        check(p.first, p.second);
    }

    // ----- Dynamic vs Static -----
    //   각 dynamic body 의 AABB 가 걸치는 cell 의 static body 들을 candidate
    //   로 잡아 narrow 검사. 동일 (dyn, static) 쌍이 두 번 검사되지 않도록
    //   QueryAABB 가 정렬 + dedup 해서 반환.
    //   - i < j 정규화는 check 안에서 자동 (dynamic id 와 static id 비교).
    std::vector<int> staticCandidates;
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        if (!alive_[i]) continue;
        const PhysicsBody& b = bodies_[i];
        if (b.type == BodyType::Static) continue;
        AABB bd{b.pos, b.shape == ShapeType::Circle ? Vec2{b.half.x, b.half.x}
                                                    : b.half};
        staticGrid_.QueryAABB(bd, staticCandidates);
        for (int s : staticCandidates) {
            // (i, s) 정규화 — naive 와 동일 순서로 push 되도록 min/max.
            int lo = std::min(static_cast<int>(i), s);
            int hi = std::max(static_cast<int>(i), s);
            check(lo, hi);
        }
    }

    // ----- 결정론 가드: contact 정렬 -----
    //   broad-phase 분리 (dynamic 페어 → static 페어) 로 두 그룹의 push 순서가
    //   naive 의 단일 i<j 순서와 다를 수 있다. ResolveAll 의 부동소수 누적이
    //   같은 contact set 라도 처리 순서에 미세 영향 → P4 의 byte-equal replay
    //   가 깨질 위험. (a, b) 사전식 정렬로 naive 와 동일 순서 보장.
    std::sort(contactsThisStep_.begin(), contactsThisStep_.end(),
              [](const Contact& x, const Contact& y) {
                  if (x.a != y.a) return x.a < y.a;
                  return x.b < y.b;
              });
}

// ----- 응답 반복 -----

void PhysicsWorld::ResolveAll() {
    // 1) 속도 임펄스: resolutionIters_ 회 반복.
    //    다중 contact (예: 바닥 + 벽 모서리에 동시에 닿은 body) 의 속도를
    //    번갈아 풀어 수렴시킨다. ResolveVelocity 는 이미 분리 중인 쌍에
    //    무동작이라 반복 호출에 안전.
    for (int iter = 0; iter < resolutionIters_; ++iter) {
        for (Contact& c : contactsThisStep_) {
            ResolveVelocity(c, bodies_[c.a], bodies_[c.b]);
        }
    }

    // 2) 위치 보정: step 당 정확히 1 회.
    //    [튕김 버그 root-cause fix] 과거에는 위치 보정이 속도 응답과 묶여
    //    resolutionIters_ 회 반복됐는데, penetration 은 DetectContacts 시점에
    //    한 번만 측정되고 반복 사이 갱신되지 않는다. 같은 침투값으로 4 회
    //    0.8 배씩 밀어내면 한 step 에 약 3.2 배 과보정 → Dynamic body 가 지면 /
    //    벽에서 과도하게 튕겨 나갔다. 위치 보정을 1 회로 분리해 의도한
    //    kPercent (0.8) 만큼만 보정한다.
    for (Contact& c : contactsThisStep_) {
        ResolvePosition(c, bodies_[c.a], bodies_[c.b]);
    }
}

// ----- Step orchestration -----

void PhysicsWorld::Step(float dt) {
    // 순서가 중요:
    //   1) 힘 적용 + 적분으로 위치 / 속도 미리 갱신.
    //   2) 갱신된 위치로 충돌 검출.
    //   3) 검출된 contact 들에 대해 응답 (위치 보정 + 속도 변경) iters 회.
    //   4) 게임 측이 등록한 콜백에 결과 통지.
    //
    // 콜백을 응답 후에 부르는 이유: 게임 측 코드 (데미지 처리 등) 가 봤을
    // 때 위치가 이미 합의된 상태가 되도록.
    ApplyForcesAndIntegrate(dt);
    DetectContacts();
    ResolveAll();

    if (onContactBegin_) {
        for (const Contact& c : contactsThisStep_) {
            onContactBegin_(c);
        }
    }
}
