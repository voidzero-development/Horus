#pragma once

class Entity;
struct UserCmd;
struct Vector;

namespace Ragebot
{
    void autoStop(UserCmd* cmd) noexcept;
    void run(UserCmd*) noexcept;

    struct Enemies {
        int id;
        int health;
        float distance;
        float fov;

        bool operator<(const Enemies& enemy) const noexcept {
            if (health != enemy.health)
                return health < enemy.health;
            if (fov != enemy.fov)
                return fov < enemy.fov;
            return distance < enemy.distance;
        }

        Enemies(int id, int health, float distance, float fov) noexcept : id(id), health(health), distance(distance), fov(fov) { }
    };

    struct {
        bool operator()(Enemies a, Enemies b) const
        {
            return a.health > b.health;
        }
    } healthSort;

    struct {
        bool operator()(Enemies a, Enemies b) const
        {
            return a.distance > b.distance;
        }
    } distanceSort;

    struct {
        bool operator()(Enemies a, Enemies b) const
        {
            return a.fov > b.fov;
        }
    } fovSort;

    void updateInput() noexcept;
}