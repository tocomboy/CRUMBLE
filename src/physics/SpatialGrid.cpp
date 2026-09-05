// =============================================================================
// src/physics/SpatialGrid.cpp — 균일 grid broad-phase 구현.
// =============================================================================

#include "SpatialGrid.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

SpatialGrid::SpatialGrid(float cellSize, Vec2 worldMin, Vec2 worldMax)
    : cellSize_(cellSize),
      worldMin_(worldMin),
      worldMax_(worldMax)
{
    // cell 수 = ceil((worldMax - worldMin) / cellSize). 최소 1.
    //   - cellSize 가 0 또는 음수이면 안전망으로 1 cell 화 (사용자 실수 방지).
    if (cellSize_ <= 0.0f) {
        cellSize_ = 1.0f;
    }
    cols_ = std::max(1, static_cast<int>(
        std::ceil((worldMax_.x - worldMin_.x) / cellSize_)));
    rows_ = std::max(1, static_cast<int>(
        std::ceil((worldMax_.y - worldMin_.y) / cellSize_)));
    cells_.assign(static_cast<std::size_t>(cols_ * rows_), {});
}

void SpatialGrid::Clear() {
    // 모든 cell 의 vector 비움. capacity 는 유지되어 다음 Insert 의 push_back 이
    // 재할당 없이 빠름.
    for (auto& c : cells_) c.clear();
}

void SpatialGrid::Insert(int bodyId, const AABB& bounds) {
    // bound 의 min/max 좌표 계산.
    const Vec2 mn = bounds.center - bounds.half;
    const Vec2 mx = bounds.center + bounds.half;

    // 월드 좌표 → cell 인덱스. 음수 / grid 밖 좌표는 clamp 로 안전 처리.
    int cx0 = std::max(0, static_cast<int>(
        std::floor((mn.x - worldMin_.x) / cellSize_)));
    int cy0 = std::max(0, static_cast<int>(
        std::floor((mn.y - worldMin_.y) / cellSize_)));
    int cx1 = std::min(cols_ - 1, static_cast<int>(
        std::floor((mx.x - worldMin_.x) / cellSize_)));
    int cy1 = std::min(rows_ - 1, static_cast<int>(
        std::floor((mx.y - worldMin_.y) / cellSize_)));

    // 모든 걸치는 cell 에 등록. 큰 body 는 여러 cell 에 자연스럽게 등장.
    for (int cy = cy0; cy <= cy1; ++cy) {
        for (int cx = cx0; cx <= cx1; ++cx) {
            cells_[CellIdx(cx, cy)].push_back(bodyId);
        }
    }
}

void SpatialGrid::QueryPairs(std::vector<std::pair<int,int>>& out) const {
    out.clear();
    // 1) 모든 cell 의 (i, j) 쌍을 raw 에 모은다 — i < j 정규화.
    //    같은 두 body 가 여러 cell 에 동시 등장하면 raw 에 중복 entry.
    std::vector<std::pair<int,int>> raw;
    for (const auto& c : cells_) {
        const std::size_t n = c.size();
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                int a = std::min(c[i], c[j]);
                int b = std::max(c[i], c[j]);
                raw.emplace_back(a, b);
            }
        }
    }
    // 2) sort + unique 로 dedup. 결과는 정렬된 (a, b) 페어 — 결정론 보장.
    std::sort(raw.begin(), raw.end());
    raw.erase(std::unique(raw.begin(), raw.end()), raw.end());
    out = std::move(raw);
}

void SpatialGrid::QueryAABB(const AABB& bounds, std::vector<int>& out) const {
    out.clear();
    const Vec2 mn = bounds.center - bounds.half;
    const Vec2 mx = bounds.center + bounds.half;
    const int cx0 = std::max(0, static_cast<int>(
        std::floor((mn.x - worldMin_.x) / cellSize_)));
    const int cy0 = std::max(0, static_cast<int>(
        std::floor((mn.y - worldMin_.y) / cellSize_)));
    const int cx1 = std::min(cols_ - 1, static_cast<int>(
        std::floor((mx.x - worldMin_.x) / cellSize_)));
    const int cy1 = std::min(rows_ - 1, static_cast<int>(
        std::floor((mx.y - worldMin_.y) / cellSize_)));

    // 걸치는 cell 들의 body id 를 모두 수집 + dedup 후 정렬.
    std::vector<int> raw;
    for (int cy = cy0; cy <= cy1; ++cy) {
        for (int cx = cx0; cx <= cx1; ++cx) {
            const auto& c = cells_[CellIdx(cx, cy)];
            raw.insert(raw.end(), c.begin(), c.end());
        }
    }
    std::sort(raw.begin(), raw.end());
    raw.erase(std::unique(raw.begin(), raw.end()), raw.end());
    out = std::move(raw);
}
