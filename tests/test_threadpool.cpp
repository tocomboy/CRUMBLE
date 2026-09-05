// =============================================================================
// tests/test_threadpool.cpp — ThreadPool 단위 테스트.
// =============================================================================
//
// 검증 대상:
//   src/resource/ThreadPool.h / .cpp 의 세 가지 계약.
//     1) threadpool_submit_runs_job      : Submit 한 잡이 실제로 실행된다.
//     2) threadpool_pending_count        : PendingCount 가 잡 수를 정확히 반영.
//     3) threadpool_drain_on_destroy     : 소멸자가 잡을 모두 실행한 뒤 종료.
//
// 출처:
//   - [2. Parallel Programming.pdf p.21-24] 잡 큐 / worker thread 패턴.
//   - 메인 플랜 P10 Task 10.1 테스트 명세.
//
// 타이밍 방식:
//   - busy-wait (atomic + yield) 로 결과를 기다린다.
//   - 최대 대기 시간을 1000ms / 2000ms 로 제한해 무한 hang 방지.
//   - 시간 초과 시 CR_ASSERT(false) 로 명시적 실패 처리.
// =============================================================================

#include "test_runner.h"
#include "assert_eq.h"
#include "../src/resource/ThreadPool.h"

#include <atomic>
#include <chrono>
#include <thread>

// -----------------------------------------------------------------------------
// Test 1: threadpool_submit_runs_job
//   Submit 한 잡이 worker thread 에서 실제로 실행되는지 검증.
//   - ran 플래그를 atomic<bool> 으로 두고 잡에서 true 로 설정.
//   - 최대 1000ms busy-wait 후 CR_ASSERT(ran).
// -----------------------------------------------------------------------------
REGISTER_TEST(threadpool_submit_runs_job) {
    ThreadPool pool(2);
    std::atomic<bool> ran{false};

    pool.Submit([&ran] {
        ran.store(true, std::memory_order_release);
    });

    // 최대 1000ms 대기 (worker 가 잡을 실행할 시간 충분히 부여).
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(1000);
    while (!ran.load(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() > deadline) {
            CR_ASSERT(false);   // 1000ms 내에 잡이 실행되지 않음 → 실패.
        }
        std::this_thread::yield();
    }
    CR_ASSERT(ran.load());
}

// -----------------------------------------------------------------------------
// Test 2: threadpool_pending_count
//   PendingCount 가 잡 제출 후 정확한 수를 반환하고, 모두 완료 후 0 이 되는지.
//   - 5ms sleep 잡 10개를 제출.
//   - 제출 직후 count == 10 이어야 한다 (worker 2개가 즉시 2개는 실행 시작
//     가능하지만, 제출과 카운트 사이 시간이 아주 짧으므로 실용적으로 10에 가까움).
//   - 최대 2000ms 대기 후 PendingCount() == 0 이어야 한다.
//
// 주의:
//   - "제출 직후 == 10" 검증은 race 가 있을 수 있으므로 완료 후 == 0 만 엄격히
//     검증하고, 제출 후 값은 "1 이상" 으로 완화한다.
// -----------------------------------------------------------------------------
REGISTER_TEST(threadpool_pending_count) {
    ThreadPool pool(2);
    std::atomic<int> completed{0};
    constexpr int kJobCount = 10;

    for (int i = 0; i < kJobCount; ++i) {
        pool.Submit([&completed] {
            // 5ms sleep — worker 가 겹쳐 실행되도록 잡당 처리 시간 부여.
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            completed.fetch_add(1, std::memory_order_release);
        });
    }

    // 제출 직후 pending 이 1 이상이어야 한다 (0이면 즉시 완료라 검증 의미 없음).
    CR_ASSERT(pool.PendingCount() >= 1 || completed.load() == kJobCount);

    // 최대 2000ms 대기 후 모두 완료.
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(2000);
    while (pool.PendingCount() > 0) {
        if (std::chrono::steady_clock::now() > deadline) {
            CR_ASSERT(false);   // 2000ms 내에 완료되지 않음 → 실패.
        }
        std::this_thread::yield();
    }

    CR_ASSERT(pool.PendingCount() == 0);
    CR_ASSERT(completed.load() == kJobCount);
}

// -----------------------------------------------------------------------------
// Test 3: threadpool_drain_on_destroy
//   소멸자가 큐에 남은 잡을 버리지 않고 모두 실행한 뒤 종료하는지 검증.
//   - scoped block 안에서 ThreadPool 을 만들고 5개의 10ms sleep 잡 제출.
//   - 블록 종료 시 소멸자 호출 → drain.
//   - 블록 종료 후 completed == 5 이어야 한다.
// -----------------------------------------------------------------------------
REGISTER_TEST(threadpool_drain_on_destroy) {
    std::atomic<int> completed{0};
    constexpr int kJobCount = 5;

    {
        // 이 scope 를 벗어날 때 소멸자가 호출되어 모든 잡이 완료된다.
        ThreadPool pool(2);
        for (int i = 0; i < kJobCount; ++i) {
            pool.Submit([&completed] {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                completed.fetch_add(1, std::memory_order_release);
            });
        }
        // 소멸자: drain-then-stop — 큐가 빌 때까지 worker 실행 후 join.
    }

    // 소멸자 반환 후이므로 completed == kJobCount 가 반드시 true.
    CR_ASSERT(completed.load() == kJobCount);
}
