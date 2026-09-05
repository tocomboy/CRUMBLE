// =============================================================================
// src/resource/AsyncLoader.cpp — 단일 worker thread 의 자원 로드 큐 구현.
// =============================================================================

#include "AsyncLoader.h"

AsyncLoader::AsyncLoader() : worker_([this] { Loop(); }) {}

AsyncLoader::~AsyncLoader() {
    // 표준 stop-the-thread 시퀀스: flag → notify → join.
    //   1) running_ 을 false 로 두면 Loop 의 wait condition 이 worker 에게
    //      종료 신호임을 알림.
    //   2) cv_.notify_all 로 worker 가 wait 에서 깨어나 조건 재검사.
    //   3) join 으로 worker 의 종료를 기다림 — 소멸자가 끝나기 전 thread 가
    //      안전하게 회수됨.
    running_ = false;
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void AsyncLoader::Enqueue(Job j) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        jobs_.push(std::move(j));
        ++pending_;
    }
    // 큐에 새 항목이 생겼으니 worker 깨움 — 한 번에 한 worker 만 처리하므로
    // notify_one 으로 충분 (notify_all 은 중복 깨움이라 비효율).
    cv_.notify_one();
}

bool AsyncLoader::IsIdle()       const { return pending_.load() == 0; }
int  AsyncLoader::PendingCount() const { return pending_.load(); }

void AsyncLoader::Loop() {
    while (running_.load()) {
        Job j;
        {
            std::unique_lock<std::mutex> lk(mtx_);
            // wait: 종료 신호이거나 큐에 항목이 있을 때까지 sleep.
            //   - spurious wakeup 은 lambda predicate 가 다시 true 일 때만 통과.
            cv_.wait(lk, [&] { return !running_.load() || !jobs_.empty(); });
            // 종료 신호 + 큐 비어있음 → Loop 종료.
            if (!running_.load() && jobs_.empty()) return;
            j = std::move(jobs_.front());
            jobs_.pop();
        }
        // 락 해제 후 job 실행 — 잡 안에서 Enqueue 호출해도 데드락 회피.
        if (j) j();
        --pending_;
    }
}
