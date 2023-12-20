#include "dijkstraMapGen.h"

#include "dungeonUtils.h"
#include "ecsTypes.h"
#include "math.h"

static bool is_visible(const DungeonData &dd, int x1, int y1, int x2, int y2) {
  auto cur_x = x1, cur_y = y1;
  while (cur_x != x2 || cur_y != y2) {
    if (dd.tiles[cur_y * dd.width + cur_x] == dungeon::wall) return false;
    int delta_x = x2 - cur_x;
    int delta_y = y2 - cur_y;
    if (abs(delta_x) > abs(delta_y))
      cur_x += signum(delta_x);
    else
      cur_y += signum(delta_y);
  }
  return true;
}

template <typename Callable>
static void query_dungeon_data(flecs::world &ecs, Callable c) {
  static auto dungeonDataQuery = ecs.query<const DungeonData>();

  dungeonDataQuery.each(c);
}

template <typename Callable>
static void query_characters_positions(flecs::world &ecs, Callable c) {
  static auto characterPositionQuery = ecs.query<const Position, const Team>();

  characterPositionQuery.each(c);
}

constexpr float invalid_tile_value = 1e5f;

static void init_tiles(std::vector<float> &map, const DungeonData &dd) {
  map.resize(dd.width * dd.height);
  for (float &v : map) v = invalid_tile_value;
}

// scan version, could be implemented as Dijkstra version as well
static void process_dmap(std::vector<float> &map, const DungeonData &dd) {
  bool done = false;
  auto getMapAt = [&](size_t x, size_t y, float def) {
    if (x < dd.width && y < dd.width &&
        dd.tiles[y * dd.width + x] == dungeon::floor)
      return map[y * dd.width + x];
    return def;
  };
  auto getMinNei = [&](size_t x, size_t y) {
    float val = map[y * dd.width + x];
    val = std::min(val, getMapAt(x - 1, y + 0, val));
    val = std::min(val, getMapAt(x + 1, y + 0, val));
    val = std::min(val, getMapAt(x + 0, y - 1, val));
    val = std::min(val, getMapAt(x + 0, y + 1, val));
    return val;
  };
  while (!done) {
    done = true;
    for (size_t y = 0; y < dd.height; ++y)
      for (size_t x = 0; x < dd.width; ++x) {
        const size_t i = y * dd.width + x;
        if (dd.tiles[i] != dungeon::floor) continue;
        const float myVal = getMapAt(x, y, invalid_tile_value);
        const float minVal = getMinNei(x, y);
        if (minVal < myVal - 1.f) {
          map[i] = minVal + 1.f;
          done = false;
        }
      }
  }
}

void dmaps::gen_player_approach_map(flecs::world &ecs, std::vector<float> &map,
                                    int range) {
  query_dungeon_data(ecs, [&](const DungeonData &dd) {
    init_tiles(map, dd);
    query_characters_positions(ecs, [&](const Position &pos, const Team &t) {
      if (t.team == 0) {
        for (int add_x = -range; add_x <= range; ++add_x) {
          for (int add_y = -range; add_y <= range; ++add_y) {
            int tx = pos.x + add_x, ty = pos.y + add_y;
            if (tx >= 0 && tx < dd.width && ty >= 0 && ty < dd.height &&
                dd.tiles[ty * dd.width + tx] == dungeon::floor &&
                is_visible(dd, pos.x, pos.y, tx, ty) &&
                L1_dist(pos.x, pos.y, tx, ty) <= range) {
              map[ty * dd.width + tx] = 0;
            }
          }
        }
      }
    });
    process_dmap(map, dd);
  });
}

void dmaps::gen_player_flee_map(flecs::world &ecs, std::vector<float> &map) {
  gen_player_approach_map(ecs, map);
  for (float &v : map)
    if (v < invalid_tile_value) v *= -1.2f;
  query_dungeon_data(ecs,
                     [&](const DungeonData &dd) { process_dmap(map, dd); });
}

void dmaps::gen_hive_pack_map(flecs::world &ecs, std::vector<float> &map) {
  static auto hiveQuery = ecs.query<const Position, const Hive>();
  query_dungeon_data(ecs, [&](const DungeonData &dd) {
    init_tiles(map, dd);
    hiveQuery.each([&](const Position &pos, const Hive &) {
      map[pos.y * dd.width + pos.x] = 0.f;
    });
    process_dmap(map, dd);
  });
}

void dmaps::gen_explore_map(flecs::world &ecs, std::vector<float> &map) {
  static auto tile_query = ecs.query<const Position, const IsExplored>();
  query_dungeon_data(ecs, [&](const DungeonData &dd) {
    init_tiles(map, dd);
    tile_query.each([&](const Position &pos, const IsExplored &explored) {
      if (!explored.value && dungeon::is_tile_walkable(ecs, pos)) {
        map[pos.y * dd.width + pos.x] = 0.f;
      }
    });
    process_dmap(map, dd);
  });
}

void dmaps::gen_ally_map(flecs::world &ecs, std::vector<float> &map,
                         flecs::entity target) {
  static auto ally_query = ecs.query<const Position, const Team>();
  query_dungeon_data(ecs, [&](const DungeonData &dd) {
    init_tiles(map, dd);
    target.get([&](const Team &targetTeam) {
      ally_query.each(
          [&](flecs::entity e, const Position &pos, const Team &team) {
            if (e != target && team.team == targetTeam.team) {
              map[pos.y * dd.width + pos.x] = 0.f;
            }
          });
    });
    process_dmap(map, dd);
  });
}