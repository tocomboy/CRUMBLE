// =============================================================================
// src/physics/Collision.cpp — narrow-phase 검출 구현.
//
// 본 단계 (P1) 에서는 DetectAABB 만 본 구현. Circle / AABB-Circle 은 P4 에서
// 활성화. Narrow dispatch 도 AABB-AABB 한정.
// =============================================================================

#include "Collision.h"
#include "PhysicsBody.h"

#include <algorithm>     // std::max / std::min — P4 의 AABB-Circle clamping.
#include <cmath>         // std::fabs / std::sqrt — 거리 / 정규화.

// AABB-AABB: 표준 separating axis 검사.
//   - d = b.pos - a.pos 두 중심의 변위.
//   - x 축 침투 깊이 = (a.halfX + b.halfX) - |d.x|.
//     양수면 x 축에서는 겹침. 0 이하면 분리됨 (즉시 false 반환).
//   - y 축 동일.
//   - 두 축 모두 양수면 → 침투 깊이가 더 작은 축이 "탈출하기 가장 가까운 방향".
//     그 축을 normal 로, 그 침투 깊이를 penetration 으로 설정한다.
//
// 출처: 일반 게임 물리 텍스트의 표준 AABB SAT 패턴. 강의 [7. Simulation.pdf]
//       해당 페이지 도착 시 정확한 인용으로 보강 예정.
bool DetectAABB(const PhysicsBody& a, const PhysicsBody& b, Contact& out) {
    // 두 중심의 변위.
    Vec2 d = b.pos - a.pos;

    // x 축 침투. 음수 / 0 이면 분리됨 → 즉시 종료.
    // (touching 정확히 0 도 분리로 간주: 강의 PDF 의 strict-less-than 관습과
    //  동일 — 테스트 aabb_touching_no_overlap 가 검증.)
    float overlapX = (a.half.x + b.half.x) - std::fabs(d.x);
    if (overlapX <= 0.0f) {
        return false;
    }

    // y 축 침투.
    float overlapY = (a.half.y + b.half.y) - std::fabs(d.y);
    if (overlapY <= 0.0f) {
        return false;
    }

    // 두 축 모두 겹침 → "최소 침투 축" 이 탈출 방향이 된다.
    // 일반적으로 두 박스가 한쪽으로 살짝 밀려 겹친 경우가 흔한데, 그쪽 축을
    // normal 로 잡아야 시각적으로 자연스러운 위치 보정이 된다.
    if (overlapX < overlapY) {
        // x 축이 더 얕음 → x 방향으로 분리. d.x 부호로 a 가 b 의 왼/오른쪽인지
        // 판단. d.x >= 0 이면 b 가 a 의 오른쪽 → normal a→b 는 (+1, 0).
        out.normal      = { (d.x < 0.0f) ? -1.0f : 1.0f, 0.0f };
        out.penetration = overlapX;
    } else {
        // y 축이 더 얕음 (또는 동률). 동률 시 y 우선 — 게임에서는 보통 중력
        // 축이라 y 우선 처리가 시각적으로 안정적.
        out.normal      = { 0.0f, (d.y < 0.0f) ? -1.0f : 1.0f };
        out.penetration = overlapY;
    }

    // 충돌 점 추정: 두 중심의 중점. P4 에서 정밀화 (clip 등) 가능.
    out.point       = a.pos + d * 0.5f;

    // 상대 속도 캐시: Resolution 이 separating velocity 검사에 사용.
    out.relativeVel = b.vel - a.vel;

    return true;
}

// =============================================================================
// Circle-Circle 검출 (P4 에서 활성화).
//
// 알고리즘:
//   - d = b.pos - a.pos.
//   - distSq = |d|².
//   - rSum = a.r + b.r. rSum² 와 distSq 비교 (sqrt 회피).
//   - distSq >= rSum² 이면 분리 — 즉시 false.
//   - 그 외:
//       dist = sqrt(distSq).
//       normal = d / dist (정규화). dist 가 거의 0 (concentric) 이면 fallback
//                = (1, 0) 단위 벡터 — 0/0 NaN 회피 + 결정론 보장.
//       penetration = rSum - dist.
//       point       = a.pos + normal * a.r (a 표면의 normal 방향 점).
//       relativeVel = b.vel - a.vel.
//
// 출처: 메인 플랜 P4 Task 4.1. 강의 [7. Simulation.pdf] 일반 충돌 패턴.
//        Plan Revisability: Week 11 Physics Engine 1 도착 시 정확한 페이지
//        인용으로 보강 가능.
// =============================================================================
bool DetectCircle(const PhysicsBody& a, const PhysicsBody& b, Contact& out) {
    Vec2 d = b.pos - a.pos;
    float distSq = d.LengthSq();
    float rSum   = a.half.x + b.half.x;       // half.x 가 반지름.

    // sqrt 호출 회피 — distSq vs rSum² 로 빠른 분리 검사.
    if (distSq >= rSum * rSum) return false;

    float dist = std::sqrt(distSq);
    if (dist > 1e-6f) {
        // 일반 케이스: d 를 dist 로 나눠 정규화.
        out.normal = { d.x / dist, d.y / dist };
    } else {
        // concentric (정확히 같은 위치) — 0/0 NaN 회피.
        // 임의 fallback (1, 0). 결정론 보장: 같은 입력이면 항상 (1, 0) 반환.
        out.normal = { 1.0f, 0.0f };
    }
    out.penetration = rSum - dist;
    // 충돌 점: a 표면의 normal 방향 점 — 시각화 / 디버그 화살표가 두 원의
    // 접점에서 시작하도록.
    out.point       = a.pos + out.normal * a.half.x;
    out.relativeVel = b.vel - a.vel;
    return true;
}

// =============================================================================
// AABB-Circle 검출 (P4 에서 활성화).
//
// 알고리즘 (closest-point clamping):
//   - aabb 중심 기준 d = circle.pos - aabb.pos.
//   - aabb 의 half 범위 안으로 d 를 clamp → closest 가 aabb 위 (또는 안) 의
//     원에 가장 가까운 점.
//   - circle.pos - closest 가 분리 벡터. 길이 < r 이면 충돌.
//
// 정규화 / fallback 은 Circle-Circle 과 동일 패턴.
//
// 출처: 메인 플랜 P4 Task 4.2.
// =============================================================================
bool DetectAABBCircle(const PhysicsBody& aabb, const PhysicsBody& circle, Contact& out) {
    Vec2 d = circle.pos - aabb.pos;
    // aabb 로컬 좌표에서 [-half, +half] 로 clamp.
    float clampedX = std::max(-aabb.half.x, std::min(d.x, aabb.half.x));
    float clampedY = std::max(-aabb.half.y, std::min(d.y, aabb.half.y));
    // 다시 월드 좌표로.
    Vec2 closest = aabb.pos + Vec2{clampedX, clampedY};
    Vec2 diff    = circle.pos - closest;
    float distSq = diff.LengthSq();
    float r      = circle.half.x;
    if (distSq >= r * r) return false;

    float dist = std::sqrt(distSq);
    if (dist > 1e-6f) {
        out.normal = { diff.x / dist, diff.y / dist };
    } else {
        // 원 중심이 aabb 안에 있는 케이스 — closest == circle.pos. fallback
        // (1, 0) 으로 NaN 회피. 시각적으로 가장 자연스러운 탈출 방향은 본
        // 단계에서 정확히 계산하지 않음 (P4 단계의 단순성 우선). 필요 시
        // P5/P10 폴리싱에서 "가장 가까운 면" 계산으로 정밀화 가능.
        out.normal = { 1.0f, 0.0f };
    }
    out.penetration = r - dist;
    out.point       = closest;
    out.relativeVel = circle.vel - aabb.vel;
    return true;
}

// dispatch — 두 body 의 shape 조합에 따라 적절한 Detect* 호출.
//   - P4 부터 4 가지 조합 (AABB-AABB / AABB-Circle / Circle-AABB / Circle-Circle)
//     모두 라우팅. Circle-AABB 는 AABB-Circle 의 normal 을 반대로 뒤집어
//     a→b 방향 일관성 유지.
bool Narrow(const PhysicsBody& a, const PhysicsBody& b, Contact& out) {
    if (a.shape == ShapeType::AABB && b.shape == ShapeType::AABB) {
        return DetectAABB(a, b, out);
    }
    if (a.shape == ShapeType::Circle && b.shape == ShapeType::Circle) {
        return DetectCircle(a, b, out);
    }
    if (a.shape == ShapeType::AABB && b.shape == ShapeType::Circle) {
        return DetectAABBCircle(a, b, out);
    }
    if (a.shape == ShapeType::Circle && b.shape == ShapeType::AABB) {
        // 인자 순서를 (aabb, circle) 로 바꿔 호출. 결과 normal 은 aabb→circle
        // 방향이므로 a→b 일관성을 위해 부호 반전.
        bool hit = DetectAABBCircle(b, a, out);
        if (hit) out.normal = -out.normal;
        return hit;
    }
    return false;
}
