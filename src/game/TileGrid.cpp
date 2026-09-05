// =============================================================================
// src/game/TileGrid.cpp — 파괴 가능한 타일 격자 구현.
// =============================================================================

#include "TileGrid.h"

#include <cstddef>

#include "../physics/PhysicsBody.h"

namespace {

// 단일 타일에 대응하는 정적 AABB body 정의 — Load / Restore 의 공통 헬퍼.
//   - kTileSize 의 절반을 half 로.
//   - mass=0 (Static), restitution=0, friction 은 default.
//   - P8 collision layer wiring: bit 3 (Static terrain). mask = ~0u 로 모든
//     layer 와 충돌. main.cpp 의 정적 cover / 측벽 / 천장과 같은 비트.
PhysicsBody MakeTileBody(Vec2 center) {
    PhysicsBody def;
    def.pos         = center;
    def.shape       = ShapeType::AABB;
    def.half        = {TileGrid::kTileSize * 0.5f, TileGrid::kTileSize * 0.5f};
    def.SetMass(0.0f);
    def.restitution = 0.0f;
    def.type        = BodyType::Static;
    def.collisionLayer = 1u << 3;
    def.collisionMask  = ~0u;
    return def;
}

}  // namespace

TileGrid::TileGrid(PhysicsWorld& world)
    : world_(world) {}

void TileGrid::LoadDestructibleArea(Vec2 topLeft, Vec2 size) {
    // P9 결함 #15 fix — additive append. P9 의 JSON 맵이 한 라운드에 multiple
    // destructible 영역 (Bunker 3 개 / Hideout 3 개) 을 보유하므로 이전의 .assign
    // (덮어쓰기) 방식은 첫 N-1 영역의 alive_/bodyIds_/centers_ 를 잃어버려 그
    // 영역의 PhysicsBody 가 PhysicsWorld 에 leak ("투명한 벽") 했다. 본 메서드는
    // 이제 호출마다 새 영역의 타일을 vector 에 append → DestroyInRadius /
    // RestoreAll / Clear 가 모든 영역의 타일을 일괄 처리.
    //
    // cols_ / rows_ 는 마지막 호출의 dims 로 갱신 (회귀 가드 — 단일 영역 전제의
    // test_tile_grid.cpp 는 그대로 통과). IsAlive(col, row) 는 단일 영역 맵에서만
    // 의미 있고 multi-area 맵에서는 마지막 영역만 가리킨다 — main.cpp 는
    // AliveFlags() / Centers() 만 사용하므로 영향 없음.
    origin_ = topLeft;
    // size 가 kTileSize 정수 배수가 아니면 floor 로 축소.
    cols_ = static_cast<int>(size.x / kTileSize);
    rows_ = static_cast<int>(size.y / kTileSize);

    const std::size_t addCount = static_cast<std::size_t>(cols_ * rows_);
    alive_.reserve(alive_.size() + addCount);
    bodyIds_.reserve(bodyIds_.size() + addCount);
    centers_.reserve(centers_.size() + addCount);

    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            // 타일 중심 = 좌상단 + (col*size + size/2, row*size + size/2).
            const Vec2 center{
                topLeft.x + c * kTileSize + kTileSize * 0.5f,
                topLeft.y + r * kTileSize + kTileSize * 0.5f
            };
            centers_.push_back(center);
            alive_.push_back(true);
            bodyIds_.push_back(world_.CreateBody(MakeTileBody(center)));
        }
    }
    // P7 staticGrid_ rebuild 트리거 — 다음 Step 의 DetectContacts 가 본 타일들
    // 을 grid 에 등록.
    world_.MarkStaticDirty();
}

int TileGrid::DestroyInRadius(Vec2 center, float radius,
                              std::function<void(Vec2)> onDestroyed) {
    int n = 0;
    const float r2 = radius * radius;
    // 인덱스 순회 — 콜백 호출 순서가 결정론 (col + row * cols_ 순).
    for (std::size_t i = 0; i < alive_.size(); ++i) {
        if (!alive_[i]) continue;
        const Vec2 d = centers_[i] - center;
        if (d.LengthSq() < r2) {
            alive_[i] = false;
            if (bodyIds_[i] >= 0) {
                world_.DestroyBody(bodyIds_[i]);
                bodyIds_[i] = -1;
            }
            if (onDestroyed) onDestroyed(centers_[i]);
            ++n;
        }
    }
    if (n > 0) {
        // 정적 body 가 변경됨 → 다음 Step 의 staticGrid_ rebuild.
        world_.MarkStaticDirty();
    }
    return n;
}

void TileGrid::Clear() {
    // 살아 있는 타일의 body 만 destroy — 이미 파괴된 (alive_=false) 타일은
    // bodyIds_=-1 이라 건너뜀. P7 의 staticGrid_ 가 다음 step rebuild.
    for (std::size_t i = 0; i < alive_.size(); ++i) {
        if (alive_[i] && bodyIds_[i] >= 0) {
            world_.DestroyBody(bodyIds_[i]);
        }
    }
    alive_.clear();
    bodyIds_.clear();
    centers_.clear();
    cols_ = 0;
    rows_ = 0;
    world_.MarkStaticDirty();
}

void TileGrid::RestoreAll() {
    for (std::size_t i = 0; i < alive_.size(); ++i) {
        if (alive_[i]) continue;
        alive_[i]   = true;
        bodyIds_[i] = world_.CreateBody(MakeTileBody(centers_[i]));
    }
    world_.MarkStaticDirty();
}

int TileGrid::AliveCount() const {
    int n = 0;
    for (bool a : alive_) if (a) ++n;
    return n;
}

bool TileGrid::IsAlive(int col, int row) const {
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_) return false;
    return alive_[col + row * cols_];
}

Vec2 TileGrid::TileCenter(int col, int row) const {
    return centers_[col + row * cols_];
}
