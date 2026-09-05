// =============================================================================
// test_pool.cpp — Pool<T> 템플릿 단위 테스트
// =============================================================================
//
// 검증 대상:
//   src/core/Pool.h 의 Pool<T> 템플릿이 다음 3 가지 핵심 계약을 지키는지.
//     1) Acquire 는 서로 다른 인덱스를 반환한다 (충돌 없음).
//     2) Release 한 슬롯은 다시 Acquire 가능하다 (재사용).
//     3) ForEachAlive 는 살아있는 슬롯만 방문한다 (죽은 슬롯 스킵).
//
// 출처 / 참고:
//   - 메인 플랜 P3 Task 3.1 Step 1 의 테스트 케이스 그대로.
//   - REGISTER_TEST 매크로는 tests/test_runner.h (P0 Task 0.4 에서 분리됨).
//   - CR_ASSERT 는 tests/assert_eq.h.
// =============================================================================
#include "test_runner.h"
#include "assert_eq.h"
#include "../src/core/Pool.h"

namespace {

// 테스트용 더미 타입 — Pool 자체의 동작만 검증하기 위해 가장 단순한 구조체.
// int 한 필드만 두어 ForEachAlive 가 살아있는 슬롯만 방문하는지 합산으로 확인.
struct Foo { int v = 0; };

}  // namespace

// -----------------------------------------------------------------------------
// Test 1: Acquire 는 서로 다른 인덱스를 반환한다.
// -----------------------------------------------------------------------------
// 풀 capacity=4 에서 두 번 Acquire 한 결과가 같으면 재사용 버그 (free_ 에서
// pop 한 슬롯을 alive 로 표시하지 않아 다음 Acquire 가 같은 슬롯을 반환).
REGISTER_TEST(pool_acquire_returns_unique_indices) {
    Pool<Foo> p;
    p.Init(4);
    int a = p.Acquire();
    int b = p.Acquire();
    CR_ASSERT(a != b);
    CR_ASSERT(a >= 0 && a < 4);
    CR_ASSERT(b >= 0 && b < 4);
}

// -----------------------------------------------------------------------------
// Test 2: 풀이 가득찬 후 Release 하면 그 슬롯이 다시 잡힌다.
// -----------------------------------------------------------------------------
// 시나리오:
//   capacity=2 에서 Acquire * 2 → 가득참 → Acquire 는 -1 → Release(a) →
//   다음 Acquire 는 a 와 같은 인덱스 반환 (LIFO 스택이라 직전 free 한 슬롯이
//   바로 재사용됨).
REGISTER_TEST(pool_release_makes_index_reusable) {
    Pool<Foo> p;
    p.Init(2);
    int a = p.Acquire();
    int b = p.Acquire();
    int c = p.Acquire();   // 풀 가득참 → -1.
    CR_ASSERT(c < 0);
    p.Release(a);
    int d = p.Acquire();
    CR_ASSERT(d == a);     // LIFO 라 방금 free 한 a 가 그대로 돌아옴.
    (void)b;               // unused 경고 회피 — b 는 시나리오 setup 용.
}

// -----------------------------------------------------------------------------
// Test 3: ForEachAlive 는 죽은 슬롯을 건너뛴다.
// -----------------------------------------------------------------------------
// 3 개 acquire 후 가운데 (b) 를 release. 합계는 1 + 3 = 4 이어야 한다 (b 의
// v=2 가 더해지면 6 이 됨). 살아있는 슬롯만 정확히 방문하는지 검증.
REGISTER_TEST(pool_for_each_visits_only_alive) {
    Pool<Foo> p;
    p.Init(3);
    int a = p.Acquire(); p.Get(a)->v = 1;
    int b = p.Acquire(); p.Get(b)->v = 2;
    int c = p.Acquire(); p.Get(c)->v = 3;
    p.Release(b);
    int sum = 0;
    p.ForEachAlive([&](int /*id*/, Foo& f) { sum += f.v; });
    CR_ASSERT(sum == 4);   // 1 (a) + 3 (c). b 는 죽었으므로 2 빠짐.
}
