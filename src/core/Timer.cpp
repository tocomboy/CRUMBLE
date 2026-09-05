// =============================================================================
// src/core/Timer.cpp — Timer 의 구현.
//
// 출처: [3. Game Loop.pdf p.8] 의 코드를 PascalCase 로 옮기고 한국어 주석을
//       라인 단위로 추가했다. 알고리즘 자체는 PDF 그대로.
// =============================================================================

#include "Timer.h"

// 생성자 초기화 리스트:
//   - freq_ 를 SDL_GetPerformanceFrequency 로 초기화.
//   - 본문에서 Start() 를 호출해 start_ 도 초기 상태로.
// 이 패턴은 PDF p.8 의 `Timer() : frequency_(SDL_GetPerformanceFrequency())
// { start(); }` 와 동일.
Timer::Timer() : freq_(SDL_GetPerformanceFrequency()) {
    Start();
}

void Timer::Start() {
    // 현재 시각을 카운터로 저장. 단위: SDL 의 platform-dependent tick.
    // freq_ 로 나누면 초 단위가 된다.
    start_ = SDL_GetPerformanceCounter();
}

float Timer::Elapsed() const {
    // 현재 시각 - 시작 시각 = 경과 tick.
    Uint64 now = SDL_GetPerformanceCounter();
    // tick → 초 환산. static_cast 로 명시적 부동소수 변환.
    //   - PDF p.8 의 (now - startCount_) / frequency_ 와 동일.
    //   - Uint64 → float 시 큰 값에서 정밀도 손실 가능하나, freq_ 로 나누고
    //     난 후 결과가 일반적으로 0~수십 초 범위라 문제 없음.
    return static_cast<float>(now - start_) / static_cast<float>(freq_);
}

float Timer::Reset() {
    // 1) 현재까지 경과 시간 측정.
    float e = Elapsed();
    // 2) 다음 측정 구간을 위해 다시 Start.
    //    Elapsed() 와 Start() 호출 사이에 지나는 nanosecond 수준의 시간은
    //    "측정 누락" 으로 처리되지만, 60 Hz / 16.67 ms 단위 측정에서는 무시
    //    가능. PDF p.8 의 reset() 도 같은 방식.
    Start();
    return e;
}
