// =============================================================================
// src/resource/ThreadPool.cpp — N-worker 범용 잡 큐 구현.
// =============================================================================
//
// 출처:
//   - [2. Parallel Programming.pdf p.21-24] thread + mutex + condition_variable
//     기반 잡 큐 패턴 (AsyncLoader.cpp 와 같은 출처).
//   - 메인 플랜 P10 Task 10.1.
// =============================================================================

#include "ThreadPool.h"

#include <algorithm>   // std::max

// -----------------------------------------------------------------------------
// 생성자: workerCount 개의 thread 를 시작.
//   - 각 thread 는 WorkerLoop 를 실행. stop_ = false 이므로 즉시 wait 상태.
//   - workerCount <= 0 이면 1 로 보정 — 잡은 제출됐는데 worker 가 없으면 영구 hang.
// -----------------------------------------------------------------------------
ThreadPool::ThreadPool(int workerCount) {
    // 최소 1개 worker 보장.
    const int n = std::max(1, workerCount);
    workers_.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        // 각 worker thread 는 this 의 WorkerLoop 를 실행.
        // [2. Parallel Programming.pdf p.22] — thread 생성 패턴.
        workers_.emplace_back([this] { WorkerLoop(); });
    }
}

// -----------------------------------------------------------------------------
// 소멸자: drain-then-stop.
//   1) stop_ = true  : WorkerLoop 의 wait 조건이 참이 되도록 표시.
//   2) notify_all    : 모든 idle worker 를 깨워 종료 경로로 진입시킴.
//   3) join          : 각 worker 가 마지막 잡까지 완료하고 리턴할 때까지 대기.
//
// AsyncLoader.cpp 와 동일한 stop 시퀀스. 차이점은 N 개 thread 를 모두 join.
// [2. Parallel Programming.pdf p.23] — join + notify_all 종료 패턴.
// -----------------------------------------------------------------------------
ThreadPool::~ThreadPool() {
    stop_.store(true, std::memory_order_relaxed);
    cv_.notify_all();   // idle 상태로 wait 중인 모든 worker 깨움.
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
}

// -----------------------------------------------------------------------------
// Submit: 잡을 큐에 추가.
//   - stop_ 상태이면 즉시 반환 (소멸자 진행 중 — 새 잡 추가 거부).
//   - pending_ 을 lock 안에서 증가시키는 이유: PendingCount() 의 관찰자가
//     "큐에 넣었는데 pending 이 아직 0" 이라는 순간을 보지 않도록.
//     단, lock-free 관찰자 (polling) 에게 완벽한 sequentiality 를 보장하려면
//     atomic fetch_add 가 충분 — 여기서는 큐 조작 lock 과 같은 critical section
//     안에서 증가해 일관성을 높임.
// -----------------------------------------------------------------------------
void ThreadPool::Submit(std::function<void()> job) {
    if (stop_.load(std::memory_order_relaxed)) return;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        tasks_.push(std::move(job));
        ++pending_;
    }
    // idle worker 하나만 깨우면 충분 (N worker 중 한 명이 가져가므로).
    cv_.notify_one();
}

// -----------------------------------------------------------------------------
// PendingCount: 잡 수를 lock-free 로 반환.
//   - 대기 중 + 실행 중 모두 포함. --pending_ 은 잡 실행 완료 후에 수행.
// -----------------------------------------------------------------------------
int ThreadPool::PendingCount() const {
    return pending_.load(std::memory_order_relaxed);
}

// -----------------------------------------------------------------------------
// WorkerLoop: 각 worker thread 가 실행하는 무한 루프.
//
// 루프 흐름 ([2. Parallel Programming.pdf p.24] 기반):
//   1) cv_.wait : stop_ || !tasks_.empty() 가 될 때까지 sleep.
//      - spurious wakeup 은 predicate 람다가 false 를 반환해 다시 wait.
//   2) 종료 조건: stop_ == true && tasks_.empty() → 루프 탈출.
//      - drain-then-stop: stop_ 이어도 큐에 잡이 남아 있으면 계속 처리.
//   3) 잡을 pop 한 뒤 lock 해제 → 잡 실행 → --pending_.
//      - 잡 실행 중 lock 을 잡지 않아 다른 worker 가 동시에 다른 잡 실행 가능.
// -----------------------------------------------------------------------------
void ThreadPool::WorkerLoop() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lk(mtx_);
            // wait: 종료 신호이거나 큐에 잡이 있을 때 통과.
            cv_.wait(lk, [this] {
                return stop_.load(std::memory_order_relaxed) || !tasks_.empty();
            });
            // 종료 신호 + 큐 비어있음 → 루프 탈출 (drain 완료).
            if (stop_.load(std::memory_order_relaxed) && tasks_.empty()) return;
            // 큐에서 잡 하나 꺼냄.
            job = std::move(tasks_.front());
            tasks_.pop();
        }
        // lock 해제 후 잡 실행 — 잡 안에서 Submit 호출해도 데드락 없음.
        if (job) job();
        // 잡 완료 후 카운터 감소. 호출자가 PendingCount()==0 을 폴링하는 패턴에 사용.
        --pending_;
    }
}
