// =============================================================================
// src/game/BreakableSlab.cpp — BreakableSlabField 구현.
// =============================================================================

#include "BreakableSlab.h"

#include "../physics/PhysicsBody.h"

namespace {

// 슬래브 하나에 대응하는 Static AABB body 정의 — Load / RestoreAll 공통 헬퍼.
//   - TileGrid::MakeTileBody 와 같은 패턴:
//       mass=0 (Static), restitution=0, collisionLayer=1u<<3, mask=~0u.
//   - SlabDef 의 center / half 를 그대로 사용 (크기가 타일마다 다름).
PhysicsBody MakeSlabBody(const SlabDef& def) {
    PhysicsBody b;
    b.pos            = def.center;
    b.shape          = ShapeType::AABB;
    b.half           = def.half;
    b.SetMass(0.0f);
    b.restitution    = 0.0f;
    b.type           = BodyType::Static;
    // bit 3 = Static terrain layer (TileGrid 와 동일 레이어).
    // mask = ~0u → 모든 레이어와 충돌.
    b.collisionLayer = 1u << 3;
    b.collisionMask  = ~0u;
    return b;
}

}  // namespace

// -----------------------------------------------------------------------------
// Load
// -----------------------------------------------------------------------------
void BreakableSlabField::Load(PhysicsWorld& world,
                              const std::vector<SlabDef>& defs) {
    // 이전 상태를 덮어쓰기 전에 살아 있는 body 를 먼저 정리.
    //   - Clear 를 그대로 재사용해 중복 코드 없이 정리.
    Clear(world);

    const std::size_t n = defs.size();
    defs_.reserve(n);
    bodyIds_.reserve(n);
    alive_.reserve(n);
    currentHp_.reserve(n);

    for (const SlabDef& def : defs) {
        defs_.push_back(def);
        alive_.push_back(true);
        currentHp_.push_back(def.hp);
        bodyIds_.push_back(world.CreateBody(MakeSlabBody(def)));
    }

    if (n > 0) {
        // 정적 body 가 추가됨 → 다음 Step 의 staticGrid_ rebuild.
        world.MarkStaticDirty();
    }
}

// -----------------------------------------------------------------------------
// Clear — 살아 있는 body 제거 + vector 비움.
// -----------------------------------------------------------------------------
void BreakableSlabField::Clear(PhysicsWorld& world) {
    for (std::size_t i = 0; i < alive_.size(); ++i) {
        // alive 인 body 만 destroy — 이미 파괴된 것은 bodyIds_[i]==-1.
        if (alive_[i] && bodyIds_[i] >= 0) {
            world.DestroyBody(bodyIds_[i]);
        }
    }
    defs_.clear();
    bodyIds_.clear();
    alive_.clear();
    currentHp_.clear();

    if (!defs_.empty()) {
        // 위에서 clear 했으므로 이 분기는 실행되지 않지만, 안전을 위해 보존.
        world.MarkStaticDirty();
    }
    // body 가 있었다면 dirty 트리거 — clear 전 크기로 판단할 수 없으므로
    // 항상 호출하는 것이 안전. MarkStaticDirty 는 idempotent.
    world.MarkStaticDirty();
}

// -----------------------------------------------------------------------------
// RestoreAll — 모든 슬래브를 라운드 시작 상태로 복원.
// -----------------------------------------------------------------------------
void BreakableSlabField::RestoreAll(PhysicsWorld& world) {
    for (std::size_t i = 0; i < defs_.size(); ++i) {
        if (alive_[i]) continue;  // 이미 살아 있는 슬래브는 건드리지 않음.

        // body 재생성 + hp 원복.
        alive_[i]     = true;
        currentHp_[i] = defs_[i].hp;
        bodyIds_[i]   = world.CreateBody(MakeSlabBody(defs_[i]));
    }
    world.MarkStaticDirty();
}

// -----------------------------------------------------------------------------
// OnExplosion — 폭발 반경에 닿은 alive 슬래브 처리.
// -----------------------------------------------------------------------------
int BreakableSlabField::OnExplosion(Vec2 center, float radius,
                                    PhysicsWorld& world,
                                    const std::function<void(Vec2)>& onDebris) {
    int destroyed = 0;
    const float r2 = radius * radius;

    // 인덱스 순 순회 → 결정론 보장 (TileGrid::DestroyInRadius 와 동일 패턴).
    for (std::size_t i = 0; i < defs_.size(); ++i) {
        if (!alive_[i]) continue;

        // AABB-원 overlap 검사.
        //   슬래브 AABB 의 closest point 를 구한 뒤 center 까지 거리² 비교.
        //   closest.x = clamp(center.x, minX, maxX) (y 마찬가지).
        const float minX = defs_[i].center.x - defs_[i].half.x;
        const float maxX = defs_[i].center.x + defs_[i].half.x;
        const float minY = defs_[i].center.y - defs_[i].half.y;
        const float maxY = defs_[i].center.y + defs_[i].half.y;

        // std::clamp 대신 인라인 min/max — <algorithm> 추가 없이 결정론 유지.
        const float cx = (center.x < minX) ? minX : (center.x > maxX) ? maxX : center.x;
        const float cy = (center.y < minY) ? minY : (center.y > maxY) ? maxY : center.y;

        const Vec2  diff{center.x - cx, center.y - cy};
        if (diff.LengthSq() >= r2) continue;  // 폭발 반경 밖 → 스킵.

        // 폭발 반경 안 → hp 감소.
        currentHp_[i] -= 1;

        if (currentHp_[i] > 0) continue;  // 아직 hp 남음 → 즉시 파괴 아님.

        // hp <= 0 → 슬래브 파괴.
        alive_[i] = false;
        if (bodyIds_[i] >= 0) {
            world.DestroyBody(bodyIds_[i]);
            bodyIds_[i] = -1;
        }

        // onDebris: 슬래브 전체 AABB 를 kDebrisStep 격자로 순회하며 호출.
        //   - 슬래브가 크면 더 많은 debris 포인트 → 시각적으로 전체 면적이 부서짐.
        //   - 격자 시작 = AABB 의 좌상단 + kDebrisStep/2 (반 칸 오프셋 → 경계
        //     안쪽에서 시작해 균일 분포).
        if (onDebris) {
            const float startX = minX + kDebrisStep * 0.5f;
            const float startY = minY + kDebrisStep * 0.5f;
            for (float py = startY; py <= maxY; py += kDebrisStep) {
                for (float px = startX; px <= maxX; px += kDebrisStep) {
                    onDebris(Vec2{px, py});
                }
            }
        }

        ++destroyed;
    }

    if (destroyed > 0) {
        // 정적 body 가 제거됨 → 다음 Step staticGrid_ rebuild.
        world.MarkStaticDirty();
    }
    return destroyed;
}

// -----------------------------------------------------------------------------
// Render — alive 슬래브를 단색 채운 사각형으로 그림.
// -----------------------------------------------------------------------------
void BreakableSlabField::Render(SDL_Renderer* r) const {
    for (std::size_t i = 0; i < defs_.size(); ++i) {
        if (!alive_[i]) continue;

        // 물리 body 의 AABB 와 정확히 일치하는 픽셀 사각형.
        //   center - half = 좌상단,  half * 2 = 크기.
        //   int 캐스팅: float → 정수 픽셀 (SDL_Rect 는 int).
        SDL_Rect rect{
            static_cast<int>(defs_[i].center.x - defs_[i].half.x),
            static_cast<int>(defs_[i].center.y - defs_[i].half.y),
            static_cast<int>(defs_[i].half.x * 2.0f),
            static_cast<int>(defs_[i].half.y * 2.0f)
        };

        // SlabDef 의 색상으로 채우기. alpha=255 (불투명).
        SDL_SetRenderDrawColor(r, defs_[i].r, defs_[i].g, defs_[i].b, 255);
        SDL_RenderFillRect(r, &rect);
        // 테두리 없음 — 지시사항: NO border.
    }
}

// -----------------------------------------------------------------------------
// AliveCount
// -----------------------------------------------------------------------------
int BreakableSlabField::AliveCount() const {
    int n = 0;
    for (bool a : alive_) if (a) ++n;
    return n;
}
