#pragma once
#include "types.hpp"
#include <vector>
#include <memory>
#include <array>

// Forward declaration
class GameObject;

struct Rect {
    float x, y, w, h;
    bool contains(Vec2 p) const {
        return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;
    }
    bool intersects(const Rect& o) const {
        return !(o.x > x + w || o.x + o.w < x || o.y > y + h || o.y + o.h < y);
    }
};

class QuadTree {
    static constexpr int CAPACITY = 8;
    static constexpr int MAX_LEVELS = 5;

    Rect boundary;
    int level;
    std::vector<GameObject*> objects;
    std::array<std::unique_ptr<QuadTree>, 4> children;
    bool divided = false;

public:
    QuadTree(Rect boundary, int level = 0) : boundary(boundary), level(level) {}

    void subdivide() {
        if (children[0]) {
            divided = true;
            return;
        }
        float x = boundary.x;
        float y = boundary.y;
        float w = boundary.w / 2.0f;
        float h = boundary.h / 2.0f;

        children[0] = std::make_unique<QuadTree>(Rect{x + w, y, w, h}, level + 1); // NE
        children[1] = std::make_unique<QuadTree>(Rect{x, y, w, h}, level + 1);     // NW
        children[2] = std::make_unique<QuadTree>(Rect{x, y + h, w, h}, level + 1); // SW
        children[3] = std::make_unique<QuadTree>(Rect{x + w, y + h, w, h}, level + 1); // SE
        divided = true;
    }

    bool insert(GameObject* obj) {
        if (!boundary.contains(obj->pos)) return false;

        if (objects.size() < CAPACITY || level >= MAX_LEVELS) {
            objects.push_back(obj);
            return true;
        }

        if (!divided) subdivide();

        for (auto& child : children) {
            if (child->insert(obj)) return true;
        }
        return false; // Should not happen for points within boundary
    }

    void query(Rect range, std::vector<GameObject*>& found) const {
        if (!boundary.intersects(range)) return;

        for (auto obj : objects) {
            if (range.contains(obj->pos)) {
                found.push_back(obj);
            }
        }

        if (divided) {
            for (auto& child : children) {
                child->query(range, found);
            }
        }
    }

    void clear() {
        objects.clear();
        if (divided) {
            for (auto& child : children) child->clear();
            // Optimization: Do NOT destroy children or set divided=false.
            // We keep the structure to avoid reallocation.
        }
    }
};
