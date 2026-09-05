// =============================================================================
// src/game/Player.h — 플레이어 (Dynamic body + FSM + 입력 처리).
//
// 목적:
//   - PhysicsWorld 안에 Dynamic body 한 개를 가지고, InputManager 의
//     액션 입력으로 좌/우 이동 + 점프 + 점프 버퍼 + 코요테 타임을 처리.
//   - PlayerState FSM (Grounded / Airborne / WallSlide / MortarAiming) 을
//     관리. 본 P2 단계에서는 Grounded ↔ Airborne 두 상태만 의미.
//
// 본 P2 단계 범위:
//   - Grounded / Airborne 전환만 (WallSlide 는 P6, MortarAiming 은 P5).
//   - 좌/우 이동 + 점프 + 점프 버퍼 (kJumpBuf) + 코요테 타임 (kCoyote).
//   - Variable-height jump (점프 떼면 상승 속도 컷).
//
// 한 프레임의 호출 순서 (main.cpp):
//   1. input.BeginFrame() / PumpFromSDL
//   2. p.UpdateInput(dt)        — 키 입력으로 force/vel 갱신
//   3. world.Step(dt)            — 물리 시뮬레이션 (충돌 검출 + 응답 +
//                                  ContactCallback → p.EvaluateContact)
//   4. p.UpdateFSM(dt)           — 충돌 결과 (hasTopContactThisStep_) 로
//                                  state_ 업데이트
//
// 출처:
//   - 메인 플랜 §3.1 Player 모듈 + FSM.
//   - [5. Input Handling.pdf p.4-8] 입력 처리 패턴.
//   - 코요테 타임 / 점프 버퍼는 일반 플랫포머의 표준 UX 패턴 — [5. Input
//     Handling.pdf] 가 명시적으로 다룰 가능성 (PDF 후반부 도착 시 정렬).
//
// 결정론:
//   - UpdateInput 은 PhysicsBody.vel 을 직접 변경하지 않는 게 원칙이지만,
//     점프 시 kJumpImp 를 직접 vel.y 에 대입한다 (메인 플랜 명시 — "direct
//     set for predictability"). ApplyImpulse 와 비슷하지만 누적이 아니라
//     덮어쓰기라 점프 높이가 일정해진다.
//   - 다른 입력 (좌/우) 은 ApplyForce 사용.
// =============================================================================

#pragma once

#include <string>
#include <utility>

#include "../math/Vec2.h"
#include "../physics/PhysicsWorld.h"
#include "../physics/PhysicsBody.h"
#include "../input/InputManager.h"
#include "PlayerStats.h"

// Forward declaration — Mortar 의 자세한 정의 (kReloadSec 등) 가 본 헤더에서
// 필요하지 않다. Player.cpp 가 Mortar.h 를 include 해 본문에서 메서드 호출.
//   - 헤더 의존을 줄여 빌드 시간 / 재컴파일 폭을 작게 유지.
class Mortar;

// PlayerState — 4 가지 상태. 본 P2 에서는 앞 두 개만 사용.
//   - Grounded     : 발이 무언가 위에 닿아 있음.
//   - Airborne     : 공중. 떨어지거나 점프 중.
//   - WallSlide    : 벽에 붙어 있음. P6 에서 활성화.
//   - MortarAiming : 박격포 조준 모드. P5 에서 활성화. 이 상태에서는 body
//                    가 일시적으로 Kinematic 으로 전환.
enum class PlayerState {
    Grounded,
    Airborne,
    WallSlide,
    MortarAiming
};

class Player {
public:
    // 생성자: 공유 PhysicsWorld 와 InputManager 의 참조 보유.
    //   - world: PhysicsBody 를 풀에 등록.
    //   - input: 액션 이름 ("p1" + "_jump" 같은 식) 으로 키 입력 쿼리.
    //   - actionPrefix: "p1" 또는 "p2" — Bindings 의 액션 이름 전반부.
    //   - spawnPos: 초기 위치 + 리스폰 위치 (PlayerStats.spawnPoint 에 저장).
    Player(PhysicsWorld& world,
           const InputManager& input,
           std::string actionPrefix,
           Vec2 spawnPos);

    // 매 fixed-timestep 호출 (world.Step 직전).
    //   - 키 입력을 읽어 좌/우 force, jump impulse, jump buffer, coyote time
    //     관리.
    //   - PhysicsBody 의 forceAccum / vel 을 직접 변경.
    void UpdateInput(float dt);

    // 매 fixed-timestep 호출 (world.Step 직후).
    //   - 직전 step 의 충돌 콜백이 hasTopContactThisStep_ 를 채워두면
    //     Grounded 로 전환. 아니면 Airborne.
    //   - 다음 frame 의 contact 정보를 리셋 (callback 이 다시 채울 수 있게).
    //   - 코요테 타이머 감소.
    void UpdateFSM(float dt);

    // PhysicsWorld::onContactBegin_ 에서 호출되는 콜백.
    //   - 본 contact 가 자기 자신을 포함하면 normal 의 y 부호로
    //     "위쪽 / 아래쪽 / 측면" 판단.
    //   - normal 이 자기로부터 상대 방향으로 +y > 0.7 이면 자기 발 밑에
    //     접촉이 있다는 의미 (Grounded).
    //   - +0.7 임계는 약 45° 이내의 위쪽 방향만 ground 로 인정 (벽 경사면을
    //     벽이 아닌 바닥으로 잘못 보지 않도록).
    void EvaluateContact(const Contact& c);

    // ----- 접근자 -----
    PlayerState  GetState() const { return state_; }
    PhysicsBody* GetBody() { return world_.GetBody(bodyId_); }
    const PhysicsBody* GetBody() const { return world_.GetBody(bodyId_); }
    BodyId       GetBodyId() const { return bodyId_; }
    PlayerStats& Stats() { return stats_; }
    const PlayerStats& Stats() const { return stats_; }

    // 위치를 spawnPoint 로 강제 + 속도 / 누적값 모두 초기화. 라운드 시작 +
    // 낙사 시 호출.
    void Respawn();

    // ----- P5: 박격포 조준 모드 / facing 관리 -----
    //
    // SetMortarRef: main.cpp 가 Mortar 인스턴스를 만든 후 본 함수로 주입.
    //   - 본 함수 호출 전까지 mortar_ == nullptr 이라 박격포 관련 동작은
    //     안전하게 건너뜀 (모두 nullptr 가드).
    void SetMortarRef(Mortar* m) { mortar_ = m; }

    // SetFacingSign: 좌/우 부호 (+1 / -1). UpdateInput 의 좌/우 입력에
    //   따라 자동 갱신되지만, 외부 (main.cpp 의 라운드 시작 시 마주보는
    //   기본 방향 설정) 도 호출 가능.
    //   - mortar_ 가 nullptr 가 아니면 aim_.facingSign 도 동기 갱신.
    void SetFacingSign(int s);

    int  FacingSign() const { return facingSign_; }

    // EnterMortarAiming: 박격포 조준 모드 진입.
    //   - state_ = MortarAiming.
    //   - PhysicsBody::type 을 일시적으로 Kinematic 으로 전환 → 외부 임펄스
    //     (다른 발사체) / 중력 영향 차단. 메인 스펙 §3.1 의 명시 동작.
    //   - vel / forceAccum / impulseAccum 모두 0 으로 — 진입 직전에 떠다니던
    //     상태가 release 시점까지 잔존하지 않게.
    //   - aim_.Reset() — phase 를 0 부터 다시 시작.
    void EnterMortarAiming();

    // ExitMortarAiming: 박격포 조준 모드 해제 (발사 완료 또는 무기 전환).
    //   - PhysicsBody::type 을 Dynamic 으로 복귀 → 다음 step 부터 중력/충돌
    //     응답 정상.
    //   - state_ = Airborne — 다음 UpdateFSM 가 grounded 검사 후 Grounded
    //     로 자연 전이.
    void ExitMortarAiming();

    // TryFireMortar: release 시점에 호출. mortar_->TryFire 가 spawn 성공하면
    //   ExitMortarAiming 으로 자동 정리.
    //   - 본 함수가 mortar_ nullptr 이거나 state_ 가 MortarAiming 이 아닐 때
    //     호출되면 noop (외부 입력 처리에서 보호).
    void TryFireMortar();

    // 외부 (main.cpp 의 발사 분기) 가 박격포 조준 중인지 검사 — 조준 중이면
    // 다른 무기 발사 입력을 무시하는 등의 분기.
    bool IsMortarAiming() const { return state_ == PlayerState::MortarAiming; }

private:
    // 공유 자원 — 참조로 보유 (소유권 없음).
    PhysicsWorld&        world_;
    const InputManager&  input_;

    // "p1" / "p2" — 액션 이름 prefix.
    std::string          prefix_;

    // PhysicsWorld 안의 자기 BodyId.
    BodyId               bodyId_;

    // 게임 측 상태.
    PlayerStats          stats_;

    // FSM 현재 상태. 시작은 Airborne — 아직 floor 에 닿지 않은 상태.
    PlayerState          state_ = PlayerState::Airborne;

    // ----- 입력 보조 타이머 -----
    // coyoteTimer: ground 를 떠난 직후에도 잠시 점프 가능하게 만드는 타임.
    //   - Grounded 일 때 매 프레임 kCoyote 로 리필.
    //   - Airborne 가 되면 dt 만큼 감소. 0 이상이면 점프 허용.
    float coyoteTimer_ = 0.0f;
    // jumpBuffer: 점프 키를 누른 직후 "잠시 동안 점프 의도 보존".
    //   - 키 누른 순간 kJumpBuf 로 리필.
    //   - 매 프레임 dt 감소. 양수 + canJump 면 트리거.
    //   - 효과: 착지 직전에 미리 점프 키를 눌러도 착지 즉시 점프 발동.
    float jumpBuffer_  = 0.0f;

    // ----- 한 프레임 contact 플래그 -----
    // EvaluateContact 가 채우고 UpdateFSM 가 읽고 리셋.
    bool  hasTopContactThisStep_   = false;
    int   wallContactSignThisStep_ = 0;     // -1 좌측 벽, +1 우측 벽 (P6).

    // ----- P6: Wall slide / Wall jump 상태 -----
    // wallJumpsAvailable_:
    //   - 새 wall contact (WallSlide 진입) 시 1 로 refresh. wall jump 1회 소모.
    //   - ground 착지 시에도 1 로 refresh — 일반 점프와 호환.
    //   - 의도: "한 벽에 한 번 차고 떼" UX. 같은 벽에 다시 붙으면 새 contact 로 인정 → 추가 1회.
    int   wallJumpsAvailable_ = 0;
    // wallSlideSign_:
    //   - WallSlide 진입 / 갱신 시 wallContactSignThisStep_ 의 부호를 보존.
    //   - 다음 frame 의 UpdateInput (wall jump 분기) / UpdateFSM (grace 동안의
    //     holdingIntoWall 검사) 가 사용. UpdateFSM 끝에서 reset 되는
    //     wallContactSignThisStep_ 와 달리 WallSlide 가 풀릴 때까지 유지.
    //   - WallSlide 가 풀리는 분기에서 0 으로 clear.
    int   wallSlideSign_ = 0;
    // wallSlideGraceTimer_:
    //   - WallSlide 진입 시 kWallSlideGrace (100 ms) 로 충전. 매 frame dt 감소.
    //   - 이유: AABB resolution 의 position correction 이 한 step 에 wall 과
    //     player 를 즉시 분리시키고, 다음 step 의 force-driven 재접근 사이에
    //     contact-가-없는 frame 이 끼어든다. 진짜 떨어지는 것은 아니므로
    //     0~100 ms 안의 contact 누락은 무시하고 WallSlide 유지.
    //   - 본 grace 가 만료되거나 키 입력이 wall 방향이 아니면 Airborne 으로.
    //   - P2 의 ground coyote (jitter 흡수) 와 같은 코드 정합 패턴 (Plan 결함 #11).
    float wallSlideGraceTimer_ = 0.0f;

    // ----- P5: 박격포 / facing -----
    // Mortar 인스턴스 reference (외부 소유). nullptr 면 박격포 기능 미사용.
    Mortar* mortar_     = nullptr;
    // 좌/우 부호. P3 까지 main.cpp 가 외부 변수로 관리하던 것을 P5 에서
    // Player 멤버로 이전. 정지 시에도 마지막 방향 유지.
    int     facingSign_ = +1;

    // ----- 튜닝 상수 -----
    // P5 / P10 의 F8 슬라이더가 추후 런타임 조정할 수 있는 후보들.
    static constexpr float kAccel   = 2400.0f;   // 좌/우 가속 계수.
    static constexpr float kMaxVx   = 300.0f;    // 좌/우 최대 속도.
    static constexpr float kJumpImp = 420.0f;    // 점프 시 vel.y 직접 대입값
                                                  //   (음수 — 화면 위쪽).
    static constexpr float kCoyote  = 0.10f;     // 100 ms.
    static constexpr float kJumpBuf = 0.10f;     // 100 ms.

    // ----- P6: Wall slide / Wall jump 튜닝 상수 -----
    // kWallSlideMaxFall:
    //   - WallSlide 상태에서 vel.y 의 상한. 자유낙하 시 ~600+ 까지 가속하지만
    //     벽에 붙으면 마찰로 천천히 미끄러져 내리는 시각적 표현.
    //   - 메인 플랜 §3.1 의 명시값 (120 px/s).
    static constexpr float kWallSlideMaxFall = 120.0f;
    // kWallJumpHorz:
    //   - Wall jump 시 벽 반대 방향 vel.x 의 절댓값. kMaxVx (300) 보다 약간
    //     작게 잡아 "튕겨나간다" 는 강제력 + 공중 제어 권한 일부 회복.
    static constexpr float kWallJumpHorz     = 280.0f;
    // kWallJumpVert:
    //   - Wall jump 시 vel.y 의 절댓값 (위로). kJumpImp (420) 보다 작게 잡아
    //     "벽 차서 사선 위로 도약" 의 느낌. 단순 점프보다 약함 + 수평 성분이
    //     보강.
    static constexpr float kWallJumpVert     = 380.0f;
    // kWallSlideGrace:
    //   - WallSlide jitter 흡수 timer 길이. P2 의 kCoyote 와 같은 100ms.
    //   - 너무 길면 벽에서 떨어진 후에도 슬라이드 효과가 남아 부자연. 너무
    //     짧으면 진동을 흡수 못함. 100ms 는 P2 ground jitter 와 동일 검증 시간.
    static constexpr float kWallSlideGrace   = 0.10f;
};
