// =============================================================================
// src/resource/ThreadPool.h — N-worker 범용 잡 큐 (Thread Pool).
// =============================================================================
//
// 목적:
//   - AsyncLoader 가 단일 worker 인 반면, ThreadPool 은 N 개의 worker thread
//     를 두어 독립적인 잡을 병렬 소화한다.
//   - F12 스냅샷 직렬화 / 파일 쓰기처럼 CPU + IO 가 섞인 잡을 메인 스레드에서
//     분리할 때 사용 (P10 디버그 빌드 전용 기능).
//   - 메인 플랜의 비협상 불변식 ("단일 스레드 결정론 물리 코어") 를 지키려면
//     Submit 된 잡이 PhysicsWorld 에 절대 접근하지 않아야 한다 — ThreadPool
//     자체는 그 제한을 강제하지 않으므로 호출자 책임.
//
// 인터페이스:
//   - Submit(job) : 잡 추가. stop_ 중이면 무시.
//   - PendingCount() : 아직 완료되지 않은 잡 수. atomic poll 가능.
//   - 소멸자 : 큐를 비운 뒤 모든 worker 를 join (drain-then-stop 시맨틱).
//
// 결정론 / 스레드 안전:
//   - Submit 은 mutex 잠금으로 thread-safe.
//   - PendingCount 는 atomic<int> 로 lock-free polling.
//   - 소멸자의 stop_=true → notify_all → join 시퀀스로 모든 worker 가 cleanly
//     종료됨을 보장. 큐에 남은 잡은 drain 후 종료 (잡을 버리지 않음).
//
// 출처:
//   - [2. Parallel Programming.pdf p.21-24] thread + mutex + condition_variable
//     기반 잡 큐 패턴.
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 10 Task 10.1.
// =============================================================================

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
    // workerCount 개의 worker thread 를 즉시 시작한다.
    //   - workerCount <= 0 이면 최소 1개로 클램프 (데드락 방지).
    explicit ThreadPool(int workerCount);

    // drain-then-stop: 큐를 소진한 뒤 모든 worker join.
    //   - stop_ = true + notify_all → 각 worker 가 wait 에서 깨어나
    //     남은 잡을 처리한 뒤 empty() 확인 후 종료.
    ~ThreadPool();

    // 복사 / 이동 금지 — worker thread 의 소유권이 단일.
    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Submit: 잡을 큐에 추가하고 idle worker 를 하나 깨운다.
    //   - stop_ 상태면 조용히 무시 (소멸자 진입 후 추가 금지).
    //   - job 이 nullptr 이면 undefined behavior — 호출자 책임.
    void Submit(std::function<void()> job);

    // PendingCount: 아직 완료 안 된 잡 수 (큐 대기 + 실행 중 합산).
    //   - atomic load 라 lock-free polling 가능.
    int  PendingCount() const;

private:
    std::vector<std::thread>          workers_;    // N 개 worker thread.
    std::queue<std::function<void()>> tasks_;      // 대기 중인 잡 큐.
    mutable std::mutex                mtx_;        // tasks_ 보호 뮤텍스.
    std::condition_variable           cv_;         // 새 잡 / 종료 신호.
    std::atomic<bool>                 stop_{false};// 소멸자 진입 표시.
    std::atomic<int>                  pending_{0}; // 대기 + 실행 중 잡 수.

    // 각 worker thread 가 실행하는 루프.
    //   - 잡이 생기거나 stop_ 이 될 때까지 wait.
    //   - stop_ && tasks_.empty() 면 루프 탈출 → thread 종료.
    void WorkerLoop();
};
