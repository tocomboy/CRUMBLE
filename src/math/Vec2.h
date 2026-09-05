// =============================================================================
// src/math/Vec2.h — 2D 벡터 (x, y).
//
// 목적:
//   - 위치 / 속도 / 힘 / 충격량 / 노멀 모두 동일한 Vec2 타입으로 표현.
//   - 산술 연산자 (+, -, *, +=, -=, *=, 단항 -) + 길이 / 정규화 / 내적.
//   - 게임 전역에서 가장 자주 등장하는 타입이라 인라인 + constexpr 친화적으로 작성.
//
// 출처:
//   - [8. Numerical Analysis 1.pdf p.1-2] — Vec2 struct 레이아웃 (x, y float
//     멤버, 기본 생성자, 산술 연산자) 및 자유 함수 (Dot, Cross, PerpLeft,
//     PerpRight, Reflect, Project, DistanceSq, Distance) 패턴을 따름.
//   - [8. Numerical Analysis 1.pdf p.3] — AngleToDir / DirToAngle (cos/sin,
//     atan2) 방향↔각도 변환 패턴을 따름.
//
// 결정론:
//   - 모든 연산은 IEEE-754 표준 float 산술. -ffast-math 금지 (CMakeLists.txt
//     의 결정론 보호 정책 참조).
//   - Normalized() 의 길이 0 처리 임계 1e-9f 는 수치 안정성용 — 변경 시
//     P4 의 결정론 replay 테스트 해시가 달라질 수 있다.
//
// 헤더 단독 (.cpp 는 비어 있음):
//   - 모든 메서드를 inline 으로 두어 호출 오버헤드 제거.
//   - 함수 본문 변경 시에는 의존하는 모든 .cpp 가 재컴파일되지만, 본 게임
//     규모에서는 비용 무시 가능.
// =============================================================================

#pragma once

#include <cmath>     // std::sqrt — Length() 가 사용.

// kPi: 표준 라이브러리에 M_PI 가 보장되지 않는 환경 (MSVC 등) 대비 자체 상수.
//   - constexpr 로 컴파일 타임 상수화.
//   - 본 게임에서는 박격포 각도 보간 (P5), HUD 게이지 등에서 사용.
constexpr float kPi = 3.14159265358979323846f;

struct Vec2 {
    // 멤버는 public (POD). 외부 코드가 b.pos.x 식으로 직접 접근 — getter 추가
    // 없이 값만 운반하는 단순 데이터 타입.
    float x = 0.0f;
    float y = 0.0f;

    // 기본 생성자: x=0, y=0.
    //   - constexpr → 컴파일 타임 상수 초기화 가능 (예: constexpr Vec2 zero;).
    constexpr Vec2() = default;

    // 좌표 직접 지정.
    constexpr Vec2(float ix, float iy) : x(ix), y(iy) {}

    // 산술 연산자 (binary).
    //   - 모두 비-멤버 const 메서드 형태 — 새 Vec2 반환.
    //   - 두 개 인자가 모두 const 참조라 임시 객체에서도 호출 가능.
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s)       const { return {x * s,  y * s }; }
    // 단항 -: 부호 반전 (예: -velocity 로 반대 방향).
    Vec2 operator-()              const { return {-x, -y}; }

    // 복합 대입 — 자기 자신을 갱신하고 *this 반환 (체이닝 가능).
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s)       { x *= s;   y *= s;   return *this; }

    // 길이 제곱 — 비교 / 거리 임계 검사에 자주 쓰이며 sqrt 비용을 피한다.
    //   - 예: if (rv.LengthSq() < 1e-3f) ... (정지로 간주).
    float LengthSq() const { return x * x + y * y; }

    // 길이 — sqrt 1회.
    float Length() const { return std::sqrt(LengthSq()); }

    // 정규화 — 단위 벡터 반환. 영벡터는 (0, 0) 으로 안전 처리.
    //   - 임계 1e-9f: 길이가 사실상 0 인 경우의 분모 0 방지.
    //     너무 크게 잡으면 작은 벡터가 0 으로 사라지고, 너무 작으면 NaN /
    //     Inf 가 발생. 1e-9f 는 게임 좌표 (보통 0~수천 픽셀) 범위에서 안정.
    Vec2 Normalized() const {
        float len = Length();
        return (len > 1e-9f) ? Vec2{x / len, y / len} : Vec2{0.0f, 0.0f};
    }

    // 내적 (dot product). 두 벡터의 평행 정도를 -1~1 범위로 반환 (정규화된
    // 입력 시).
    //   - 본 함수는 정적 메서드로 두어 Vec2::Dot(a, b) 형태로 사용.
    //   - 충돌 응답 (Resolution.cpp) 에서 분리 속도 검사 vN = dot(rv, normal)
    //     같은 곳에서 사용된다.
    static float Dot(const Vec2& a, const Vec2& b) {
        return a.x * b.x + a.y * b.y;
    }
};

// =============================================================================
// Vec2 비-멤버 자유 함수 (free functions)
//
// 목적:
//   - Vec2 struct 외부에서 a op b 형태의 수학 연산을 제공.
//   - 멤버 함수로는 표현이 어색한 이항(binary) / 기하(geometric) 연산을
//     자유 함수로 분리해 가독성 향상.
//   - 강의 PDF 8 (Numerical Analysis 1) 의 벡터 수학 섹션 패턴을 따름.
//
// 출처:
//   - [8. Numerical Analysis 1.pdf p.2] — 내적(dot), 외적(cross), 투영(project)
//   - [8. Numerical Analysis 1.pdf p.3] — 반사(reflect), 방향 변환(angle↔dir)
//
// 결정론:
//   - 모든 연산은 IEEE-754 표준 float. -ffast-math 금지.
//   - atan2 / cos / sin 은 표준 <cmath> 구현 — 플랫폼별 미세 차이가 있을 수
//     있으나 단일 머신 replay (P4) 에서는 문제 없다.
// =============================================================================

// Dot — 내적. a·b = ax*bx + ay*by.
//   [8. Numerical Analysis 1.pdf p.2]
//   - Vec2::Dot(a, b) 정적 메서드와 동일한 산식; 자유 함수 형태로도 제공해
//     ADL (Argument-Dependent Lookup) 로 더 자연스러운 호출이 가능.
inline float Dot(const Vec2& a, const Vec2& b) {
    return a.x * b.x + a.y * b.y;
}

// Cross — 2D 외적의 스칼라 값. a×b = ax*by - ay*bx.
//   [8. Numerical Analysis 1.pdf p.2]
//   - 2D 에서는 결과가 스칼라(Z 성분). 양수 = a 에서 b 로 반시계 방향.
//   - 충돌 접선 방향 판별, 회전 토크 계산에 사용.
inline float Cross(const Vec2& a, const Vec2& b) {
    return a.x * b.y - a.y * b.x;
}

// PerpLeft — 왼쪽 수직 벡터. v 를 반시계(CCW) 90° 회전한 벡터.
//   [8. Numerical Analysis 1.pdf p.2]
//   - 결과: {-v.y, v.x}. 예) (1,0) → (0,1).
//   - 노멀 계산, 접선 방향 이동에 활용.
inline Vec2 PerpLeft(const Vec2& v) {
    return {-v.y, v.x};
}

// PerpRight — 오른쪽 수직 벡터. v 를 시계(CW) 90° 회전한 벡터.
//   [8. Numerical Analysis 1.pdf p.2]
//   - 결과: {v.y, -v.x}. 예) (1,0) → (0,-1).
inline Vec2 PerpRight(const Vec2& v) {
    return {v.y, -v.x};
}

// Reflect — 노멀 n 에 대한 반사 벡터.
//   [8. Numerical Analysis 1.pdf p.2]
//   - 공식: v - n * (2 * dot(v, n)).
//   - n 은 단위 벡터여야 올바른 결과가 나온다.
//   - 총알 벽 반사, 폭발 충격 반사 방향 계산에 사용.
inline Vec2 Reflect(const Vec2& v, const Vec2& n) {
    return v - n * (2.0f * Dot(v, n));
}

// Project — onto 방향으로의 벡터 투영.
//   [8. Numerical Analysis 1.pdf p.2]
//   - 공식: (dot(v, onto) / dot(onto, onto)) * onto.
//   - onto 가 영벡터에 가까우면 (분모 < 1e-9f) 영벡터 반환 (NaN 방지).
//   - 충돌 응답에서 속도를 노멀/접선 성분으로 분해할 때 사용.
inline Vec2 Project(const Vec2& v, const Vec2& onto) {
    float d = Dot(onto, onto);
    if (d < 1e-9f) return {0.0f, 0.0f};  // 영벡터 onto — 투영 불가.
    return onto * (Dot(v, onto) / d);
}

// DistanceSq — 두 점 사이 거리의 제곱. sqrt 없이 비교 가능.
//   [8. Numerical Analysis 1.pdf p.2]
//   - 임계 검사 (예: 폭발 반경 내 여부) 에서 sqrt 비용을 피한다.
inline float DistanceSq(const Vec2& a, const Vec2& b) {
    return (a - b).LengthSq();
}

// Distance — 두 점 사이 유클리드 거리.
//   [8. Numerical Analysis 1.pdf p.2]
//   - sqrt 1회. 실제 거리 값이 필요한 HUD 표시 등에 사용.
inline float Distance(const Vec2& a, const Vec2& b) {
    return (a - b).Length();
}

// AngleToDir — 라디안 각도를 단위 방향 벡터로 변환.
//   [8. Numerical Analysis 1.pdf p.3]
//   - 결과: {cos(angle), sin(angle)}.
//   - 박격포 발사 방향, HUD 게이지 회전 방향 계산에 사용.
inline Vec2 AngleToDir(float angle) {
    return {std::cos(angle), std::sin(angle)};
}

// DirToAngle — 방향 벡터를 라디안 각도로 변환.
//   [8. Numerical Analysis 1.pdf p.3]
//   - 결과 범위: [-π, π] (atan2 표준).
//   - 반사 각도 표시, 입력 방향 직렬화에 사용.
inline float DirToAngle(const Vec2& dir) {
    return std::atan2(dir.y, dir.x);
}
