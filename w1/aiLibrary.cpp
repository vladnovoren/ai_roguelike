#include "aiLibrary.h"

#include <flecs.h>

#include <cfloat>
#include <cmath>

#include "ecsTypes.h"
#include "raylib.h"

template <typename T>
T sqr(T a) {
  return a * a;
}

template <typename T, typename U>
static float dist_sq(const T &lhs, const U &rhs) {
  return float(sqr(lhs.x - rhs.x) + sqr(lhs.y - rhs.y));
}

template <typename T, typename U>
static float dist(const T &lhs, const U &rhs) {
  return sqrtf(dist_sq(lhs, rhs));
}

template <typename T, typename U>
static int move_towards(const T &from, const U &to) {
  int deltaX = to.x - from.x;
  int deltaY = to.y - from.y;
  if (abs(deltaX) > abs(deltaY))
    return deltaX > 0 ? EA_MOVE_RIGHT : EA_MOVE_LEFT;
  return deltaY < 0 ? EA_MOVE_UP : EA_MOVE_DOWN;
}

void MoveToEntity(flecs::entity actor, flecs::entity target) {
  actor.set([&](Action &actor_action, Position &actor_pos) {
    target.get([&](const Position &target_pos) {
      actor_action.action = move_towards(actor_pos, target_pos);
    });
  });
}

void HealEntity(flecs::entity actor, flecs::entity target) {
  target.set([&](Hitpoints &hp) {
    actor.get([&](const HealerPoints &heal) { hp.hitpoints += heal.amount; });
  });
}

static int inverse_move(int move) {
  return move == EA_MOVE_LEFT    ? EA_MOVE_RIGHT
         : move == EA_MOVE_RIGHT ? EA_MOVE_LEFT
         : move == EA_MOVE_UP    ? EA_MOVE_DOWN
         : move == EA_MOVE_DOWN  ? EA_MOVE_UP
                                 : move;
}

template <typename Callable>
static void on_closest_enemy_pos(flecs::world &ecs, flecs::entity entity,
                                 Callable c) {
  static auto enemiesQuery = ecs.query<const Position, const Team>();
  entity.set([&](const Position &pos, const Team &t, Action &a) {
    flecs::entity closestEnemy;
    float closestDist = FLT_MAX;
    Position closestPos;
    enemiesQuery.each(
        [&](flecs::entity enemy, const Position &epos, const Team &et) {
          if (t.team == et.team) return;
          float curDist = dist(epos, pos);
          if (curDist < closestDist) {
            closestDist = curDist;
            closestPos = epos;
            closestEnemy = enemy;
          }
        });
    if (ecs.is_valid(closestEnemy)) c(a, pos, closestPos);
  });
}

template <typename Callable>
static void on_closest_teammate_pos(flecs::world &ecs, flecs::entity entity,
                                    Callable c) {
  static auto teammatesQuery = ecs.query<const Position, const Team>();
  entity.set([&](const Position &pos, const Team &t, Action &a) {
    flecs::entity closest_teammate;
    float closest_dist = FLT_MAX;
    Position closest_pos;
    teammatesQuery.each(
        [&](flecs::entity teammate, const Position &tpos, const Team &tt) {
          if (t.team != tt.team) return;
          auto cur_dist = dist(tpos, pos);
          if (cur_dist < closest_dist) {
            closest_dist = cur_dist;
            closest_pos = tpos;
            closest_teammate = teammate;
          }
        });
    if (ecs.is_valid(closest_teammate)) c(a, pos, closest_pos);
  });
}

void AttackEnemyState::enter() {}
void AttackEnemyState::exit() {}
void AttackEnemyState::act(float, flecs::world &, flecs::entity) {}

void MoveToEnemyState::enter() {}
void MoveToEnemyState::exit() {}
void MoveToEnemyState::act(float, flecs::world &ecs, flecs::entity entity) {
  on_closest_enemy_pos(
      ecs, entity,
      [&](Action &a, const Position &pos, const Position &enemy_pos) {
        a.action = move_towards(pos, enemy_pos);
      });
}

MoveToEntityState::MoveToEntityState(flecs::entity target) : target_(target) {}

void MoveToEntityState::enter() {}
void MoveToEntityState::exit() {}
void MoveToEntityState::act(float /* dt*/, flecs::world &ecs,
                            flecs::entity actor) {
  MoveToEntity(actor, target_);
}

HealEntityState::HealEntityState(flecs::entity target) : target_(target) {}

void HealEntityState::enter() {}
void HealEntityState::exit() {}
void HealEntityState::act(float /* dt*/, flecs::world &ecs,
                          flecs::entity entity) {
  HealEntity(entity, target_);
}

void FleeFromEnemyState::enter() {}
void FleeFromEnemyState::exit() {}
void FleeFromEnemyState::act(float /* dt*/, flecs::world &ecs,
                             flecs::entity entity) {
  on_closest_enemy_pos(
      ecs, entity,
      [&](Action &a, const Position &pos, const Position &enemy_pos) {
        a.action = inverse_move(move_towards(pos, enemy_pos));
      });
}

PatrolState::PatrolState(float dist) : patrolDist(dist) {}
void PatrolState::enter() {}
void PatrolState::exit() {}
void PatrolState::act(float /* dt*/, flecs::world &ecs, flecs::entity entity) {
  entity.set([&](const Position &pos, const PatrolPos &ppos, Action &a) {
    if (dist(pos, ppos) > patrolDist)
      a.action = move_towards(pos, ppos);  // do a recovery walk
    else {
      // do a random walk
      a.action = GetRandomValue(EA_MOVE_START, EA_MOVE_END - 1);
    }
  });
}

void NopState::enter() {}
void NopState::exit() {}
void NopState::act(float /* dt*/, flecs::world &ecs, flecs::entity entity) {}

bool TrueTransition::isAvailable(flecs::world &, flecs::entity) const {
  return true;
}

std::unique_ptr<StateTransition> TrueTransition::Copy() const {
  return std::unique_ptr<TrueTransition>();
}

EnemyAvailableTransition::EnemyAvailableTransition(float in_dist)
    : triggerDist(in_dist) {}

bool EnemyAvailableTransition::isAvailable(flecs::world &ecs,
                                           flecs::entity entity) const {
  static auto enemiesQuery = ecs.query<const Position, const Team>();
  bool enemiesFound = false;
  entity.get([&](const Position &pos, const Team &t) {
    enemiesQuery.each(
        [&](flecs::entity enemy, const Position &e_pos, const Team &et) {
          if (t.team == et.team) return;
          float curDist = dist(e_pos, pos);
          enemiesFound |= curDist <= triggerDist;
        });
  });
  return enemiesFound;
}

std::unique_ptr<StateTransition> EnemyAvailableTransition::Copy() const {
  return std::make_unique<EnemyAvailableTransition>(*this);
}

EntityNearTransition::EntityNearTransition(flecs::entity target,
                                           float thres_dist)
    : target_(target), thres_dist_(thres_dist) {}

bool EntityNearTransition::isAvailable(flecs::world &,
                                       flecs::entity actor) const {
  bool res = false;
  actor.get([&](const Position &actor_pos) {
    target_.get([&](const Position &target_pos) {
      res = (dist(actor_pos, target_pos) <= thres_dist_);
    });
  });
  return res;
}

[[nodiscard]] std::unique_ptr<StateTransition> EntityNearTransition::Copy()
    const {
  return std::make_unique<EntityNearTransition>(*this);
}

EntityLowHpTransition::EntityLowHpTransition(flecs::entity entity, float thres)
    : entity_(entity), thres_(thres) {}

bool EntityLowHpTransition::isAvailable(flecs::world &, flecs::entity) const {
  bool res = false;
  entity_.get([&](const Hitpoints &hp) { res |= hp.hitpoints < thres_; });
  return res;
}

[[nodiscard]] std::unique_ptr<StateTransition> EntityLowHpTransition::Copy()
    const {
  return std::make_unique<EntityLowHpTransition>(*this);
}
