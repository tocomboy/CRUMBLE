// =============================================================================
// src/core/Timer.h — 단조(monotonic) 벽시계 측정 헬퍼.
//
// 목적:
//   - SDL_GetPerformanceCounter 위에 얇은 래퍼를 만들어 "지난 호출 이후 몇
//     초가 지났는가" 를 단일 메서드 호출로 얻는다.
//   - 메인 루프의 frame timing, 쿨다운, 디버그 패널의 ms 측정 등에서 재사용.
//
// 출처:
//   - [3. Game Loop.pdf p.8] High-Resolution Timer Class.
//     PDF 의 클래스명 Timer / 메서드 reset() · elapsed() · elapsedMs() ·
//     start() 그대로 차용. 본 프로젝트는 메서드를 PascalCase 로 통일해
//     `Reset() / Elapsed() / Start()` 로 사용한다 (이유는
//     docs/code-explanation/00-build-and-loop.md 의 "Naming convention 노트"
//     참조).
//
// 결정론 / 스레드:
//   - SDL_GetPerformanceCounter 는 단조 증가, 스레드 안전. 그러나 본 클래스
//     인스턴스 자체는 멀티스레드에서 동시 접근하지 않는다. 메인 루프가 1 개,
//     서브시스템(디버그 패널) 마다 별도 인스턴스를 갖는 식으로 사용한다.
// =============================================================================

#pragma once

#include <SDL.h>     // Uint64, SDL_GetPerformanceCounter, SDL_GetPerformanceFrequency.

class Timer {
public:
    // 생성과 동시에 Start() 호출. 즉 "Timer t;" 한 줄로 측정 시작.
    Timer();

    // 측정 시작 시각을 현재로 재설정.
    //   - 입력: 없음.
    //   - 출력: 없음.
    //   - 부수효과: start_ 갱신.
    void Start();

    // Start() 이후 경과한 초 수 (float) 반환.
    //   - 입력: 없음.
    //   - 출력: 초 단위 경과 시간. 60 FPS 1프레임 기준 약 0.0167.
    //   - 실패조건: 없음 (SDL 초기화 실패 시 0 에 가까운 값이 나올 수 있으나
    //     이는 호출자의 환경 문제이지 본 함수의 결함이 아님).
    float Elapsed() const;

    // 경과 시간을 반환한 뒤 다음 측정을 위해 Start() 를 호출하는 편의 메서드.
    //   - 사용 예: 메인 루프 첫 줄 `float dt = timer.Reset();`
    //   - 출력: 직전 Start() ~ 지금까지의 경과 시간 (초).
    //   - PDF p.8 의 사용 예와 동일한 시그니처.
    float Reset();

private:
    // SDL_GetPerformanceCounter() 의 마지막 Start() 시점 값.
    Uint64 start_;

    // SDL_GetPerformanceFrequency() — 1초 당 카운터 증가량.
    // 생성자에서 한 번 캐시. PDF p.8 가 강조한 최적화: 매 Elapsed() 호출에서
    // SDL 함수를 다시 부르면 시스템 콜 오버헤드가 누적되므로 한 번만 읽는다.
    // 주의: 일부 시스템에서는 freq 가 런타임 중 변할 수 있다는 이론적 우려가
    // 있으나, 게임 텀 프로젝트의 단일 머신 시연에서는 무시 가능.
    Uint64 freq_;
};
