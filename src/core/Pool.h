// =============================================================================
// Pool<T> — 일반 객체 풀 (object pool) 템플릿
// =============================================================================
//
// 역할:
//   고정 용량의 T 슬롯을 미리 확보해두고, Acquire/Release 로 슬롯의 "살아있음"
//   비트만 토글한다. 새 객체를 만들 때 매번 new/delete 하지 않으므로:
//     1) 힙 단편화 회피 (게임 루프 내 빈번한 alloc 은 최악의 패턴).
//     2) 결정론 보호 — 같은 입력에 대해 같은 인덱스가 재사용되어 메모리 주소
//        편차가 시뮬레이션 결과에 영향을 주지 않는다.
//     3) iteration cache locality — 살아있는 객체들이 storage_ 의 연속 메모리
//        에 모여있으므로 ForEachAlive 가 빠르다.
//
// 출처 / 참고:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 3 Task 3.1.
//   - 게임 프로그래밍 일반 패턴 (Game Programming Patterns, "Object Pool"
//     Bob Nystrom). 본 강의 PDF 에는 풀 패턴이 직접 등장하지 않으나,
//     [3. Game Loop.pdf p.7] 의 fixed timestep 가정 위에서 "프레임마다
//     allocation 을 일정하게 유지" 라는 결정론 요구를 만족시키는 표준 도구.
//   - Plan Revisability Policy: 학기 후반 PDF 가 풀 패턴을 다른 형태 (예:
//     free-list 방식, generation counter) 로 가르치면 그 패턴으로 정렬한다.
//     현재 구현은 vector<bool> alive 비트 + vector<int> free 스택의 가장
//     단순한 형태.
//
// 설계 결정:
//   - free_ 를 stack (LIFO) 으로 운영 — Release 직후 Acquire 가 같은 슬롯을
//     재사용해 cache hot 한 슬롯을 우선 사용. 디버깅 시 인덱스 흐름이
//     예측 가능해 결정론 검증에 유리.
//   - id 는 단순 int (slot index). generation counter 는 본 게임 규모 ( <= 100
//     projectile, <= 200 chunk) 에서 ABA 문제 발생 가능성이 매우 낮아 생략.
//     필요시 P10 폴리싱 단계에서 (id, gen) tuple 로 확장 가능.
//   - storage_[i] 는 Release 시 명시적 reset 하지 않는다 — Acquire 가 다음에
//     T{} 로 덮어쓰므로 안전. 다만 외부에서 "live 가 아닌 슬롯의 데이터" 에
//     의존하면 안 된다.
//
// 사용 예:
//   Pool<Projectile> pool;
//   pool.Init(100);
//   int id = pool.Acquire();      // -1 이면 풀 가득참
//   if (id >= 0) {
//       Projectile* p = pool.Get(id);
//       p->lifetime = 0.0f;
//   }
//   pool.ForEachAlive([&](int id, Projectile& p){ p.lifetime += dt; });
//   pool.Release(id);
// =============================================================================
#pragma once

#include <cstddef>   // size_t — vector 가 transitively 가져오긴 하지만 실제
                     // 빌드 환경 (libstdc++) 에 따라 가져오지 않는 경우가
                     // 있어 명시적으로 포함. 빌드 깨짐 방지.
#include <vector>

template <typename T>
class Pool {
public:
    // -------------------------------------------------------------------------
    // Init(capacity): 풀 용량 고정. 게임 시작 시 한 번만 호출하는 설계.
    // -------------------------------------------------------------------------
    // 동적 resize 를 의도적으로 지원하지 않는다 — 실시간 게임에서 풀 크기를
    // 키우는 것은 상한 산정 (몇 발/몇 chunk?) 이 설계 단계에서 끝나야 한다는
    // 신호이다. 상한을 모르고 만든 풀은 스파이크 시 복구 불가능한 stutter 를
    // 만든다.
    void Init(int capacity) {
        // storage_ 를 capacity 크기로 default 초기화. T{} 는 모든 슬롯을 같은
        // 초기 상태로 두어 "live 가 아닌 슬롯도 정의된 값" 을 갖게 한다 —
        // valgrind 등 메모리 도구에서 '읽혀선 안 될 메모리 읽기' 오탐을 줄임.
        storage_.assign(static_cast<size_t>(capacity), T{});
        // alive_ 는 storage_ 와 1:1 대응. 같은 인덱스에 대해 alive_[i] == true
        // 이면 storage_[i] 가 의미 있는 데이터, false 면 "free 슬롯".
        alive_.assign(static_cast<size_t>(capacity), false);
        // free_ 는 free 슬롯 인덱스의 LIFO 스택. capacity-1 부터 0 까지 역순
        // push 해서 Acquire 가 첫 번째로 인덱스 0 을 반환하도록 한다 — 디버깅
        // 시 "0 부터 채운다" 가 직관적.
        free_.clear();
        free_.reserve(capacity);
        for (int i = capacity - 1; i >= 0; --i) free_.push_back(i);
    }

    // -------------------------------------------------------------------------
    // Acquire(): free 슬롯 하나를 찾아 alive 로 표시하고 그 인덱스를 반환.
    //            풀이 가득차면 -1.
    // -------------------------------------------------------------------------
    // 호출자는 반드시 반환값 >= 0 을 검사해야 한다. 반환값을 그대로 Get() 에
    // 넘기면 nullptr 을 받게 되므로 이중 안전망이 있긴 하지만, -1 을 무시하면
    // "발사를 시도했는데 풀이 가득차서 무시됨" 같은 게임 로직 버그가 조용히
    // 묻힌다 — Rifle::TryFire 처럼 명시적으로 false 를 반환해 호출자에게
    // 알려야 한다.
    int Acquire() {
        if (free_.empty()) return -1;
        int id = free_.back();
        free_.pop_back();
        alive_[id] = true;
        // T{} 로 명시 초기화 — 이전 사용자가 남긴 데이터로 인한 상태 누수를
        // 방지한다. 결정론 측면에서도 "Acquire 후 즉시 읽으면 정의된 값" 을
        // 보장.
        storage_[id] = T{};
        return id;
    }

    // -------------------------------------------------------------------------
    // Release(id): 슬롯을 free 로 되돌림.
    // -------------------------------------------------------------------------
    // 이미 free 인 슬롯에 대해 다시 Release 호출은 무시 (double-free 방어).
    // 범위 밖 id 도 무시 — 호출자가 -1 같은 invalid id 를 가지고 있을 수 있다.
    void Release(int id) {
        if (id < 0 || id >= static_cast<int>(alive_.size())) return;
        if (!alive_[id]) return;   // 이미 죽은 슬롯이면 stack 에 중복 push 안 함.
        alive_[id] = false;
        free_.push_back(id);
    }

    // -------------------------------------------------------------------------
    // Get(id): 살아있는 슬롯의 포인터를 반환. 죽었거나 범위 밖이면 nullptr.
    // -------------------------------------------------------------------------
    // 반환된 포인터는 다음 Init 호출 또는 storage_ 의 vector 재할당 (없음 —
    // capacity 고정) 까지 유효. 다른 슬롯의 Acquire/Release 는 포인터 무효화
    // 하지 않는다.
    T* Get(int id) {
        if (id < 0 || id >= static_cast<int>(storage_.size())) return nullptr;
        if (!alive_[id]) return nullptr;
        return &storage_[id];
    }

    // -------------------------------------------------------------------------
    // IsAlive(id): 슬롯 상태만 검사 (포인터 dereference 없이).
    // -------------------------------------------------------------------------
    // ContactBeginCallback 같은 곳에서 "이 projectile id 가 아직 살아있나?"
    // 를 가볍게 검사할 때 사용.
    bool IsAlive(int id) const {
        if (id < 0 || id >= static_cast<int>(alive_.size())) return false;
        return alive_[id];
    }

    // -------------------------------------------------------------------------
    // Capacity(): 풀의 고정 용량.
    // ActiveCount(): 현재 살아있는 슬롯 수 — O(N) 스캔 (capacity 작으므로 OK).
    // -------------------------------------------------------------------------
    // ActiveCount 를 별도 카운터로 유지하지 않는 이유: Acquire/Release 마다
    // 카운터를 갱신하면 코드 경로가 늘고, 100 을 스캔하는 비용은 무시 가능
    // 수준 (디버그 HUD/F7 에서만 호출).
    int Capacity()    const { return static_cast<int>(storage_.size()); }
    int ActiveCount() const {
        int n = 0;
        for (bool a : alive_) if (a) ++n;
        return n;
    }

    // -------------------------------------------------------------------------
    // ForEachAlive(fn): 살아있는 모든 슬롯에 대해 fn(id, T&) 호출.
    // -------------------------------------------------------------------------
    // fn 안에서 Release 호출은 안전하다 — 인덱스 i 를 명시적으로 진행시키므로
    // 자기 자신을 release 해도 다음 i 로 넘어간다. 단, fn 안에서 Acquire 를
    // 호출해 새 슬롯이 i 보다 작은 인덱스에 들어와도 그 슬롯은 이번 루프에서
    // 방문하지 않는다 (free_ 가 LIFO 라 직전 release 한 슬롯이 다시 잡힐 수
    // 있음 — Update 같은 곳에서 self-spawn 패턴은 피하는 것이 안전).
    template <typename F>
    void ForEachAlive(F&& fn) {
        for (size_t i = 0; i < storage_.size(); ++i) {
            if (alive_[i]) fn(static_cast<int>(i), storage_[i]);
        }
    }

private:
    std::vector<T>    storage_;   // 슬롯 본체 (capacity 고정).
    std::vector<bool> alive_;     // storage_[i] 가 살아있는지 여부.
    std::vector<int>  free_;      // free 슬롯 인덱스의 LIFO 스택.
};
