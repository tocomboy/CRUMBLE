// =============================================================================
// src/resource/AsyncLoader.h — 단일 worker thread 의 비동기 자원 로더.
// =============================================================================
//
// 목적:
//   - 게임 시작 시 모든 자산을 동기 로드하면 Title 화면 진입까지 시간이 걸려
//     검은 화면 노출 시각 부자연. 본 클래스가 자산 로드를 worker thread 로
//     보내 메인 스레드는 즉시 Title 렌더링 + "Loading..." 표시 가능.
//   - 메인 플랜의 비협상 불변식 ("단일 스레드 결정론 물리 코어") 를 지키기
//     위해 본 worker 는 PhysicsWorld 에 절대 접근하지 않는다 — 자산 로드 +
//     ResourceManager 캐시 갱신만 수행.
//
// 의존:
//   - 표준 라이브러리만 (std::thread / std::mutex / std::condition_variable).
//
// 안전:
//   - Enqueue 는 mutex 잠금으로 thread-safe.
//   - PendingCount 는 atomic 으로 lock 없이 polling.
//   - 소멸자에서 running_=false + cv_.notify_all + worker_.join 의 표준
//     stop-the-thread 시퀀스.
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 9 Task 9.2
//     Step 4.
//   - [2. Parallel Programming.pdf p.21-24] 의 thread + queue 패턴.
//
// 결정론 (비협상 불변식 보호):
//   - 본 worker 는 ResourceManager 만 만지고 PhysicsWorld 는 건드리지 않는다.
//   - 시뮬레이션과 동시 변경되는 게임 상태가 없으므로 결정론에 영향 없음.
// =============================================================================

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

class AsyncLoader {
public:
    using Job = std::function<void()>;

    AsyncLoader();
    ~AsyncLoader();
    AsyncLoader(const AsyncLoader&)            = delete;
    AsyncLoader& operator=(const AsyncLoader&) = delete;

    // 메인 스레드에서 호출. j 의 사본을 큐에 push + worker 깨움.
    void Enqueue(Job j);

    // 큐가 비고 worker 가 idle 인지. 메인 스레드의 Title 화면 polling 에 사용.
    bool IsIdle()       const;
    int  PendingCount() const;

private:
    std::thread             worker_;
    std::queue<Job>         jobs_;
    mutable std::mutex      mtx_;
    std::condition_variable cv_;
    std::atomic<bool>       running_{true};
    std::atomic<int>        pending_{0};

    void Loop();
};
