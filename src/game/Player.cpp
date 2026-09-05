// =============================================================================
// src/game/Player.cpp — Player 구현.
// =============================================================================

#include "Player.h"

#include <algorithm>

#include "weapons/Mortar.h"           // P5 — mortar_ 의 메서드 호출용.
#include "weapons/MortarAimState.h"   // P5 — aim_.facingSign 직접 접근용.

Player::Player(PhysicsWorld& world,
               const InputManager& input,
               std::string actionPrefix,
               Vec2 spawnPos)
    : world_(world),
      input_(input),
      prefix_(std::move(actionPrefix))
{
    // PhysicsBody 정의 — 플레이어는 32x48 박스, 1kg, 마찰 0.6.
    PhysicsBody def;
    def.pos         = spawnPos;
    def.shape       = ShapeType::AABB;
    def.half        = {16.0f, 24.0f};       // 32x48 직사각형 (사람 비율).
    def.SetMass(1.0f);
    def.restitution = 0.0f;                  // 점프 후 바닥에 안 튕김.
    def.friction    = 0.6f;                  // 좌/우 키 떼면 빠르게 감속.
    def.type        = BodyType::Dynamic;     // 중력 + 충돌 응답 받음.

    // P8 collision layer wiring:
    //   layer = 1<<0 (Player). mask 비트는 다음 모두 포함:
    //     bit 0 (Player)         — 두 player 가 서로 통과하지 않도록 (P3 acceptance).
    //     bit 1 (Projectile)     — 데미지 처리.
    //     bit 2 (Chunk)          — D2.5 spec — chunk 가 player 를 막거나 밀어냄.
    //     bit 3 (Static terrain) — floor / wall / cover / TileGrid.
    //     bit 4 (MovingPlatform) — Kinematic 발판.
    //
    // [Plan 결함 #13 — root-cause fix]
    //   Plan §8 Task 8.2 step 3 의 Player mask 권장값에 bit 0 (Player) 가
    //   누락되어 있다. 이 상태로 wiring 하면 두 player 끼리 mask & layer = 0
    //   이라 narrow-phase 호출 자체가 안 됨 → 두 player 가 서로 통과 → P3 의
    //   "두 player 가 마주보면 막힘" acceptance gate 회귀. mask 에 bit 0 을
    //   추가해 두 player 충돌 보존.
    def.collisionLayer = 1u << 0;
    def.collisionMask  = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4);

    bodyId_ = world_.CreateBody(def);
    stats_.spawnPoint = spawnPos;
}

void Player::UpdateInput(float dt) {
    PhysicsBody* b = world_.GetBody(bodyId_);
    if (!b) return;

    // ----- MortarAiming 분기 (P5) -----
    // 조준 모드에서는 좌/우/점프 입력을 무시하고 fire/angle 키만 처리.
    //   - fire hold      → AdvanceGauge (속도 게이지 진동).
    //   - angle hold     → AdvanceAngle (각도 진동).
    //   - fire released  → TryFireMortar (현재 위상으로 발사 + ExitMortarAiming).
    //   - weapon swap    → 박격포 취소 (OnSwitchedOut + ExitMortarAiming).
    //
    // 본 분기는 일반 분기보다 먼저 검사 — Kinematic 으로 동결된 body 가
    // 좌/우 force 누적이나 jump 임펄스를 받으면 다음 ExitMortarAiming 후
    // 의도치 않은 점프가 발생할 수 있음.
    if (state_ == PlayerState::MortarAiming) {
        const bool fireHeld     = input_.IsHeld    (prefix_ + "_fire");
        const bool fireReleased = input_.IsReleased(prefix_ + "_fire");
        const bool angleHeld    = input_.IsHeld    (prefix_ + "_mortar_angle");
        const bool weapSwap1    = input_.IsPressed (prefix_ + "_weapon_1");
        const bool weapSwap2    = input_.IsPressed (prefix_ + "_weapon_2");

        if (mortar_) {
            if (fireHeld)  mortar_->Aim().AdvanceGauge(dt);
            if (angleHeld) mortar_->Aim().AdvanceAngle(dt);
        }
        if (fireReleased) {
            // release 시점의 aim 위상으로 즉시 발사. 성공/실패 무관 — 박격포
            // 모드는 release 와 함께 종료 (탄이 없으면 그냥 빈 발사).
            TryFireMortar();
        } else if (weapSwap1 || weapSwap2) {
            // 다른 무기 키를 누르면 조준 취소. 실제 무기 전환은 main.cpp 측
            // PlayerWeapons.currentIdx 가 처리 — 본 분기는 박격포 상태만 정리.
            if (mortar_) mortar_->OnSwitchedOut();
            ExitMortarAiming();
        }
        return;
    }

    // 액션 이름 조립: prefix "_jump" 등.
    const bool left         = input_.IsHeld     (prefix_ + "_move_left");
    const bool right        = input_.IsHeld     (prefix_ + "_move_right");
    const bool jumpPressed  = input_.IsPressed  (prefix_ + "_jump");
    const bool jumpReleased = input_.IsReleased (prefix_ + "_jump");

    // ----- jumpBuffer 갱신 -----
    // 키 누른 프레임에 풀 충전, 그 외에는 dt 감소.
    // 효과: 키를 약간 일찍 눌러도 100 ms 안에 착지하면 점프 발동.
    if (jumpPressed) {
        jumpBuffer_ = kJumpBuf;
    } else {
        jumpBuffer_ = std::max(0.0f, jumpBuffer_ - dt);
    }

    // ----- Variable-height jump (점프 떼면 상승 속도 컷) -----
    // 키를 일찍 떼면 vel.y 를 -150 으로 클램프 → 작은 점프.
    // 끝까지 누르면 자연스럽게 -kJumpImp = -420 의 풀 점프.
    // 이 패턴은 일반 플랫포머의 표준 UX (Celeste 등).
    if (jumpReleased && b->vel.y < -150.0f) {
        b->vel.y = -150.0f;
    }

    // ----- Wall jump (P6) -----
    // WallSlide 상태에서 점프 키 + wallJumpsAvailable_>0 이면 벽 반대 방향
    // 사선 임펄스 (수평 ±kWallJumpHorz, 수직 -kWallJumpVert).
    //
    // sign 추정:
    //   - UpdateFSM 가 매 frame 끝에 wallContactSignThisStep_ 를 reset 하므로
    //     본 (UpdateInput) 시점에는 보통 0 이다. 따라서 facingSign_ 으로
    //     fallback — WallSlide 진입 조건 (holdingIntoWall) 상 facingSign_ 은
    //     wall sign 과 같으므로 정합 (Plan §3.1, Task 6.1 Step 3).
    //
    // 본 분기는 좌/우 이동 force 적용 전에 둔다 — wall jump 직후 dir 키가
    // wall 방향일 수 있으나, force 는 다음 step 의 적분에서 자연스럽게 공중
    // 제어로 작동 (vel 자체는 본 분기에서 set 되어 즉시 반영).
    if (state_ == PlayerState::WallSlide &&
        jumpBuffer_ > 0.0f && wallJumpsAvailable_ > 0) {
        // sign 우선순위:
        //   (1) wallContactSignThisStep_ — 본 frame 의 직전 step 에 wall 과
        //       contact 가 있었던 경우 (드물지만 가능).
        //   (2) wallSlideSign_         — UpdateFSM 가 진입 / grace 동안 보존한
        //       마지막 wall sign. 본 시점에서 가장 신뢰 가능.
        //   (3) facingSign_            — 모든 정보 손실 시 최후 fallback.
        int sign = wallContactSignThisStep_;
        if (sign == 0) sign = wallSlideSign_;
        if (sign == 0) sign = facingSign_;
        b->vel.x = -static_cast<float>(sign) * kWallJumpHorz;
        b->vel.y = -kWallJumpVert;
        --wallJumpsAvailable_;
        state_               = PlayerState::Airborne;
        jumpBuffer_          = 0.0f;
        coyoteTimer_         = 0.0f;
        wallSlideSign_       = 0;
        wallSlideGraceTimer_ = 0.0f;
    }

    // ----- 좌/우 이동 -----
    float dir = 0.0f;
    if (left)  dir -= 1.0f;
    if (right) dir += 1.0f;

    // facing 자동 갱신 (P5) — 한쪽만 눌렸을 때 그 방향으로 부호 갱신.
    // 양쪽 동시 누름 / 정지 시에는 마지막 방향 유지.
    //   - main.cpp 가 P3 에서 외부 변수로 들고 있던 p1Facing/p2Facing 의
    //     역할을 본 라인이 대신 — Mortar 의 발사 방향도 본 facingSign_ 가
    //     결정한다.
    if (right && !left) SetFacingSign(+1);
    if (left  && !right) SetFacingSign(-1);

    if (dir != 0.0f) {
        // 목표 속도 = dir × kMaxVx (방향과 절댓값).
        // 현재 속도와의 차이에 비례하는 force 를 적용 → "스냅이 잘 잡히는"
        // 운동감.
        //   - kAccel × 0.01 = 24 가 각 프레임 force 단위. dt(=1/60) 적분 후
        //     속도 변화 = 24 × diff × invMass × dt = 24 × diff × 1/60.
        //   - 정확한 가속 곡선보다 게임 감각 우선.
        const float target = dir * kMaxVx;
        const float diff   = target - b->vel.x;
        b->ApplyForce({diff * kAccel * 0.01f, 0.0f});
    }
    // 입력 없을 때는 PhysicsWorld 의 friction (0.6 dt-비례) 이 자연 감속.

    // ----- 점프 트리거 -----
    // canJump = (Grounded 상태) 이거나 (코요테 타이머 활성).
    // 코요테 타이머는 ground 에서 떨어진 직후 잠깐 유지되어 "벼랑 끝에서
    // 점프 직전에 발이 빠졌어도 점프 허용" UX.
    const bool canJump = (state_ == PlayerState::Grounded) ||
                         (coyoteTimer_ > 0.0f);
    if (jumpBuffer_ > 0.0f && canJump) {
        // vel.y 직접 대입 — 메인 플랜 §3.1 의 "direct set for predictability".
        // ApplyImpulse 누적이 아니라 덮어쓰기라 점프 높이가 항상 일정.
        b->vel.y     = -kJumpImp;
        coyoteTimer_ = 0.0f;
        jumpBuffer_  = 0.0f;
        state_       = PlayerState::Airborne;
    }
}

void Player::UpdateFSM(float dt) {
    PhysicsBody* b = world_.GetBody(bodyId_);
    if (!b) return;

    // P5 — MortarAiming 동안에는 grounded/coyote 갱신을 건너뛴다 (body 가
    // Kinematic 이라 contact 콜백이 의미 없음). ExitMortarAiming 직후의
    // 첫 frame 부터 정상 흐름 복귀.
    //   - 다음 frame 의 contact 플래그는 리셋해야 한다 (잔재 누락 방지).
    if (state_ == PlayerState::MortarAiming) {
        hasTopContactThisStep_   = false;
        wallContactSignThisStep_ = 0;
        return;
    }

    // 직전 world.Step 의 ContactCallback 이 채워둔 contact 플래그 스냅샷.
    //   - grounded         : 발 밑 위쪽-방향 normal contact.
    //   - wallContact      : 좌/우측 측면 contact (sign 으로 어느 쪽인지 판단).
    //   - falling          : 중력 진행 중 (vel.y > 0). 본 프로젝트의 y 축은
    //                        화면 아래쪽이 + 이므로 vel.y > 0 = 떨어지는 중.
    //   - holdingIntoWall  : 키 입력이 wall sign 방향과 일치 — "벽을 향해
    //                        밀고 있는" 상태일 때만 WallSlide 진입.
    //                        반대 방향 키 또는 키 없을 때는 자연 분리되어
    //                        Airborne 으로 흘러야 한다 (Plan §3.1).
    const bool grounded        = hasTopContactThisStep_;
    const bool wallContact     = (wallContactSignThisStep_ != 0);
    PhysicsBody* bodyForChecks = b;  // 위에서 이미 nullptr 검사됨.
    const bool falling         = (bodyForChecks->vel.y > 0.0f);
    const bool holdingIntoWall =
        (wallContactSignThisStep_ > 0 && input_.IsHeld(prefix_ + "_move_right")) ||
        (wallContactSignThisStep_ < 0 && input_.IsHeld(prefix_ + "_move_left"));

    // ----- 핵심: jitter 흡수 + 코요테 타임 통합 + WallSlide 분기 (P6) -----
    // 물리 jitter (kSlop 잔여 침투 → 1 frame 분리) 로 contact 가 한 frame 빠
    // 져도 매 frame 깜빡이는 Grounded↔Airborne 전환을 막아야 한다. 또한
    // "벼랑 끝에서 막 떨어진 직후 잠시 점프 허용" UX 도 같이 처리.
    //
    // 규칙 (우선순위 순):
    //   (1) grounded=true               → Grounded, coyote 풀 충전, wall jump 잔량 1로 refresh.
    //   (2) wallContact && falling && holdingIntoWall → WallSlide.
    //       - 진입 순간(전 상태 ≠ WallSlide) wallJumpsAvailable_ = 1 로 리프레시.
    //       - vel.y 를 kWallSlideMaxFall 로 cap — 중력에 휩쓸리지 않고 천천히 미끄러짐.
    //   (3) 그 외:
    //       - coyoteTimer 감소. Grounded 였으면 jitter grace 후 Airborne 으로.
    //       - WallSlide 였으면 즉시 Airborne (벽에서 떨어졌거나 키 떼었음).
    //
    // 메인 플랜 원본은 grounded=false 즉시 Airborne 이었으나 jitter 로 인해
    // 단위 테스트가 깜빡 실패 → coyoteTimer 흡수 추가는 P2 Plan Revisability
    // 코드 정합 fix. P6 의 WallSlide 분기는 그 위에 얹힌다.
    if (grounded) {
        if (state_ != PlayerState::Grounded) {
            state_              = PlayerState::Grounded;
            wallJumpsAvailable_ = 1;   // 착지 시 wall jump 1회 충전.
        }
        coyoteTimer_         = kCoyote;
        wallSlideGraceTimer_ = 0.0f;   // 착지 시 grace 폐기.
        wallSlideSign_       = 0;
    } else if (wallContact && falling && holdingIntoWall) {
        if (state_ != PlayerState::WallSlide) {
            state_              = PlayerState::WallSlide;
            wallJumpsAvailable_ = 1;   // 새 wall contact 마다 1회 refresh.
        }
        wallSlideSign_       = wallContactSignThisStep_;
        wallSlideGraceTimer_ = kWallSlideGrace;
        // gravity damp — vel.y 를 cap 까지만. 음수 (위쪽) 는 그대로 두어
        // 점프로 막 진입한 직후에도 cap 에 의해 바로 끌어내려지지 않게.
        if (bodyForChecks->vel.y > kWallSlideMaxFall) {
            bodyForChecks->vel.y = kWallSlideMaxFall;
        }
    } else {
        coyoteTimer_         = std::max(0.0f, coyoteTimer_ - dt);
        wallSlideGraceTimer_ = std::max(0.0f, wallSlideGraceTimer_ - dt);

        // ----- WallSlide grace 흡수 (Plan 결함 #11) -----
        //   AABB resolution + force ping-pong 으로 매 frame contact 보장 안 됨.
        //   진입 시 충전된 100ms timer 와, 마지막 wallSlideSign_ 기준
        //   holdingIntoWall 재검사가 모두 통과하면 WallSlide 유지 + cap 도 계속
        //   적용. 둘 중 하나라도 실패하면 Airborne 으로 빠져나간다.
        const bool stillSlideHold = (wallSlideSign_ != 0) && falling &&
            ((wallSlideSign_ > 0 && input_.IsHeld(prefix_ + "_move_right")) ||
             (wallSlideSign_ < 0 && input_.IsHeld(prefix_ + "_move_left")));

        if (state_ == PlayerState::WallSlide &&
            wallSlideGraceTimer_ > 0.0f && stillSlideHold) {
            // grace 구간 — vel.y cap 도 계속 유지 (그러지 않으면 grace 1 frame
            // 만에 자유낙하로 가속해 cap 무력화).
            if (bodyForChecks->vel.y > kWallSlideMaxFall) {
                bodyForChecks->vel.y = kWallSlideMaxFall;
            }
        } else {
            if (state_ == PlayerState::Grounded && coyoteTimer_ <= 0.0f) {
                state_ = PlayerState::Airborne;
            }
            if (state_ == PlayerState::WallSlide) {
                // 벽에서 떨어졌거나 입력을 뗐음 / grace 만료. coyote 는 갱신
                // 하지 않아 wall slide 직후 일반 점프 (Airborne+coyote) 로의
                // 전환 권한은 부여하지 않는다.
                state_         = PlayerState::Airborne;
                wallSlideSign_ = 0;
            }
        }
    }

    // 다음 프레임을 위해 contact 플래그 리셋. EvaluateContact 가 다시 채움.
    hasTopContactThisStep_   = false;
    wallContactSignThisStep_ = 0;
}

void Player::EvaluateContact(const Contact& c) {
    const int meIdx = bodyId_;
    // 본 contact 에 자기가 포함 안 되면 무관.
    if (c.a != meIdx && c.b != meIdx) {
        return;
    }

    // [Plan 결함 #12 — root-cause fix]
    //   Projectile (Rifle/Shotgun 탄 / Mortar 등) 은 spawn 위치가 player 중심.
    //   첫 step 의 ApplyForcesAndIntegrate 후에도 작은 projectile (half=3) 이
    //   player 안에 잔류하는 경우가 있어 narrow-phase 가 AABB-AABB contact 를
    //   검출 → normal=(±1, 0) 이 우측/좌측 "벽" 으로 잘못 인식되어 다음 후과:
    //     (a) 떨어지는 도중 facing 방향으로 좌/우 이동 hold + 발사 → WallSlide
    //         진입 (holdingIntoWall=true) → vel.y cap=120 → 추락 일시 감속.
    //     (b) WallSlide 진입 시 wallJumpsAvailable_=1 refresh → 점프 키 누름
    //         → wall jump 발동 → "추가 점프" 시각.
    //   Projectile 과의 contact 는 데미지 처리 (CombatHooks) 가 전적으로 담당.
    //   본 wall/ground 판정 분기는 정적 지형 / 다른 player 만 대상으로.
    //   userData != nullptr 이면 projectile 로 식별 (Projectile.cpp 가 spawn 시
    //   pj 포인터를 def.userData 에 심음).
    BodyId otherIdx = (c.a == meIdx) ? c.b : c.a;
    if (PhysicsBody* other = world_.GetBody(otherIdx)) {
        if (other->userData != nullptr) return;
    }

    // c.normal 은 a→b 방향. 내가 a 면 그대로, 내가 b 면 부호 반전 → "나에서
    // 상대로 향하는 방향" 으로 통일.
    Vec2 fromMeToOther = (c.a == meIdx)
                         ? c.normal
                         : Vec2{-c.normal.x, -c.normal.y};

    // y 가 0.7 이상 (45° 이내 위쪽) 이면 상대가 내 발 아래 — Grounded 인정.
    //   - y 가 1.0 에 가까울수록 정확한 위/아래.
    //   - 임계 0.7 ≈ cos(45°). 너무 작으면 가파른 경사면도 ground 로 잘못
    //     인식. 너무 크면 약간 기운 floor 에서 ground 검출 실패.
    if (fromMeToOther.y > 0.7f) {
        hasTopContactThisStep_ = true;
    }

    // 벽 검출 (P6 에서 본격 활용). 측면이 normal x 에 대응.
    if (fromMeToOther.x > 0.7f) {
        // 나에서 상대로 +x — 즉 상대가 내 오른쪽 → 우측 벽.
        wallContactSignThisStep_ = +1;
    } else if (fromMeToOther.x < -0.7f) {
        wallContactSignThisStep_ = -1;
    }
}

void Player::Respawn() {
    PhysicsBody* b = world_.GetBody(bodyId_);
    if (!b) return;

    // 위치 + 속도 + 누적 force/impulse 모두 초기화. 깨끗한 시작 상태.
    b->pos          = stats_.spawnPoint;
    b->vel          = Vec2{};
    b->forceAccum   = Vec2{};
    b->impulseAccum = Vec2{};
    // P5 — 라운드 도중 박격포 조준 중에 죽을 수도 있음. body type 을 항상
    // Dynamic 으로 복귀시켜 다음 라운드 시작 시 정상 흐름 보장.
    b->type         = BodyType::Dynamic;

    // FSM / 타이머 리셋.
    state_               = PlayerState::Airborne;
    coyoteTimer_         = 0.0f;
    jumpBuffer_          = 0.0f;
    wallSlideSign_       = 0;
    wallSlideGraceTimer_ = 0.0f;
    wallJumpsAvailable_  = 0;

    // P5 — Mortar 조준 위상도 리셋 (라운드 사이 잔존 방지).
    if (mortar_) mortar_->Aim().Reset();
}

// =============================================================================
// P5: 박격포 조준 / facing 관리.
// =============================================================================

void Player::SetFacingSign(int s) {
    // s 가 0 이면 +1 으로 (정의 상 ±1 만 허용). 정확히 양수면 +1, 음수면 -1.
    facingSign_ = (s >= 0) ? +1 : -1;
    // Mortar 가 연결돼 있으면 그쪽 aim 의 facingSign 도 동기. 발사 방향이
    // CurrentDirection 으로 결정되므로 좌우 미러링이 즉시 반영.
    if (mortar_) {
        mortar_->Aim().facingSign = facingSign_;
    }
}

void Player::EnterMortarAiming() {
    PhysicsBody* b = world_.GetBody(bodyId_);
    if (!b) return;

    // FSM 변경.
    state_ = PlayerState::MortarAiming;

    // body 의 모든 운동량 / 누적 force/impulse 0 으로 — 일시 정지.
    b->vel          = Vec2{};
    b->forceAccum   = Vec2{};
    b->impulseAccum = Vec2{};
    // Kinematic 으로 전환: PhysicsWorld 의 적분기 (ApplyForcesAndIntegrate)
    // 가 Kinematic body 에 대해 중력 / impulse 를 적용하지 않는다. 충돌
    // 응답에서도 무한 질량처럼 다뤄져 다른 dynamic body 가 본 body 로
    // 인해 분리된다 (메인 스펙 §3.1).
    b->type = BodyType::Kinematic;

    // aim 위상 리셋 + facing 재동기.
    if (mortar_) {
        mortar_->Aim().Reset();
        mortar_->Aim().facingSign = facingSign_;
    }
}

void Player::ExitMortarAiming() {
    PhysicsBody* b = world_.GetBody(bodyId_);
    if (!b) return;

    // body 를 Dynamic 으로 복귀 — 다음 step 부터 중력 / 충돌 응답 정상.
    b->type = BodyType::Dynamic;

    // FSM Airborne 으로 — 다음 UpdateFSM 의 grounded 검사가 즉시
    // Grounded 로 전이시킨다 (서있던 발판 위였다면).
    state_ = PlayerState::Airborne;
}

void Player::TryFireMortar() {
    if (state_ != PlayerState::MortarAiming) return;
    if (!mortar_) return;
    PhysicsBody* b = world_.GetBody(bodyId_);
    if (!b) return;

    // [Plan 결함 #14 — root-cause fix]
    //   Mortar 의 초기 속도 (kMinSpeed=400) 는 Rifle (1200) 의 1/3. spawn origin
    //   = player 중심 으로 호출하면 첫 step 후에도 mortar 가 player half (16, 24)
    //   안에 잔존 → contact 발생 → main.cpp 의 explodeMortarIfNeeded 가 즉시
    //   폭발 트리거 → spawn 직후 폭발 → "발사 자체가 안 되는" 시각.
    //
    //   Fix: spawn origin 을 발사 방향으로 (player half + Mortar.kRadius + 1)
    //   px 만큼 떨어뜨려 첫 step 부터 player 와 narrow-phase 분리 보장.
    //   - dir 의 x 성분 + player half.x 만 고려 (수직 발사 시 y 성분 무시)
    //     하면 위쪽 발사 시 player 위로 살짝 nudge — 자연스러운 "총구 위치".
    //
    //   Rifle/Shotgun 은 spawn 위치 동일 하지만 초기 속도 1200 이라 첫 step
    //   에서 ~20 px 이동해 자연 분리 → 본 fix 가 mortar 만 영향. 단, projectile
    //   self-contact 결함 #12 (P6) 의 EvaluateContact userData skip 도 함께
    //   살아있어 player 입장의 wall/ground 잘못된 인식은 모든 무기 차단.
    Vec2 dir    = mortar_->Aim().CurrentDirection();
    Vec2 origin = Vec2{
        b->pos.x + dir.x * (b->half.x + Mortar::kRadius + 1.0f),
        b->pos.y + dir.y * (b->half.y + Mortar::kRadius + 1.0f),
    };
    mortar_->TryFire(origin, dir);
    ExitMortarAiming();
}
