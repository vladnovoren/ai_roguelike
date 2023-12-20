#pragma once
#include <flecs.h>

#include <vector>

namespace dmaps {
void gen_player_approach_map(flecs::world &ecs, std::vector<float> &map,
                             int range = 0);
void gen_player_flee_map(flecs::world &ecs, std::vector<float> &map);
void gen_hive_pack_map(flecs::world &ecs, std::vector<float> &map);
void gen_explore_map(flecs::world &ecs, std::vector<float> &map);
void gen_ally_map(flecs::world &ecs, std::vector<float> &map,
                  flecs::entity target);
};  // namespace dmaps
