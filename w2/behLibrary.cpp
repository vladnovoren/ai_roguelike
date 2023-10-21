#include "aiLibrary.h"
#include "ecsTypes.h"
#include "aiUtils.h"
#include "math.h"
#include "raylib.h"
#include "blackboard.h"

struct CompoundNode : public BehNode
{
  std::vector<BehNode*> nodes;

  virtual ~CompoundNode()
  {
    for (BehNode *node : nodes)
      delete node;
    nodes.clear();
  }

  CompoundNode &pushNode(BehNode *node)
  {
    nodes.push_back(node);
    return *this;
  }
};

struct Sequence : public CompoundNode
{
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    for (BehNode *node : nodes)
    {
      BehResult res = node->update(ecs, entity, bb);
      if (res != BEH_SUCCESS)
        return res;
    }
    return BEH_SUCCESS;
  }
};

struct Selector : public CompoundNode
{
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    for (BehNode *node : nodes)
    {
      BehResult res = node->update(ecs, entity, bb);
      if (res != BEH_FAIL)
        return res;
    }
    return BEH_FAIL;
  }
};

struct Not : public CompoundNode {
  ~Not() override {
    delete node;
  }

  BehResult update(flecs::world& ecs, flecs::entity ent, Blackboard& bb) override {
    auto res = node->update(ecs, ent, bb);
    if (res == BEH_SUCCESS)
      return BEH_FAIL;
    if (res == BEH_FAIL)
      return BEH_SUCCESS;
    return BEH_RUNNING;
  }

  BehNode* node;
};

struct Parallel : public CompoundNode {
  BehResult update(flecs::world& ecs, flecs::entity ent, Blackboard& bb) override {
    for (auto node : nodes) {
      auto res = node->update(ecs, ent, bb);
      if (res != BEH_RUNNING)
        return res;
    }
    return BEH_RUNNING;
  }
};

struct MoveToEntity : public BehNode
{
  size_t entityBb = size_t(-1); // wraps to 0xff...
  MoveToEntity(flecs::entity entity, const char *bb_name)
  {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_RUNNING;
    entity.set([&](Action &a, const Position &pos)
    {
      flecs::entity targetEntity = bb.get<flecs::entity>(entityBb);
      if (!targetEntity.is_alive())
      {
        res = BEH_FAIL;
        return;
      }
      targetEntity.get([&](const Position &target_pos)
      {
        if (pos != target_pos)
        {
          a.action = move_towards(pos, target_pos);
          res = BEH_RUNNING;
        }
        else
          res = BEH_SUCCESS;
      });
    });
    return res;
  }
};

struct IsLowHp : public BehNode
{
  float threshold = 0.f;
  IsLowHp(float thres) : threshold(thres) {}

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &) override
  {
    BehResult res = BEH_SUCCESS;
    entity.get([&](const Hitpoints &hp)
    {
      res = hp.hitpoints < threshold ? BEH_SUCCESS : BEH_FAIL;
    });
    return res;
  }
};

struct FindEnemy : public BehNode
{
  size_t entityBb = size_t(-1);
  float distance = 0;
  FindEnemy(flecs::entity entity, float in_dist, const char *bb_name) : distance(in_dist)
  {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_FAIL;
    static auto enemiesQuery = ecs.query<const Position, const Team>();
    entity.set([&](const Position &pos, const Team &t)
    {
      flecs::entity closestEnemy;
      float closestDist = FLT_MAX;
      Position closestPos;
      enemiesQuery.each([&](flecs::entity enemy, const Position &epos, const Team &et)
      {
        if (t.team == et.team)
          return;
        float curDist = dist(epos, pos);
        if (curDist < closestDist)
        {
          closestDist = curDist;
          closestPos = epos;
          closestEnemy = enemy;
        }
      });
      if (ecs.is_valid(closestEnemy) && closestDist <= distance)
      {
        bb.set<flecs::entity>(entityBb, closestEnemy);
        res = BEH_SUCCESS;
      }
    });
    return res;
  }
};

struct FindBuff : public BehNode {
  FindBuff(flecs::entity ent, const char* bb_name) {
    bb_ent = reg_entity_blackboard_var<flecs::entity>(ent, bb_name);
  }

  BehResult update(flecs::world& ecs, flecs::entity ent, Blackboard& bb) override {
    BehResult res = BEH_FAIL;

    flecs::entity closest_buff;
    float closest_dist = FLT_MAX;

    auto heal_res = FindClosestWithT<HealAmount>(ecs, ent);
    if (heal_res.second < closest_dist) {
      closest_buff = heal_res.first;
      closest_dist = heal_res.second;
    }

    auto powerup_res = FindClosestWithT<PowerupAmount>(ecs, ent);
    if (powerup_res.second < closest_dist) {
      closest_buff = heal_res.first;
      closest_dist = heal_res.second;
    }

    if (ecs.is_valid(closest_buff)) {
      bb.set<flecs::entity>(bb_ent, closest_buff);
      res = BEH_SUCCESS;
    }

    return res;
  }

  template<typename T>
  std::pair<flecs::entity, float> FindClosestWithT(flecs::world& ecs, flecs::entity ent) {
    float min_dist = FLT_MAX;
    flecs::entity closest_t;

    static auto t_query = ecs.query<const Position, const T>();
    ent.get([&](const Position& ent_pos) {
      t_query.each([&](flecs::entity t_ent, const Position& t_pos, const T&) {
        float curr_dist = dist(ent_pos, t_pos);
        if (curr_dist < min_dist) {
          min_dist = curr_dist;
          closest_t = t_ent;
        }
      });
    });
    return {closest_t, min_dist};
  }

  size_t bb_ent = size_t(-1);
};

struct GetNextWaypoint : public BehNode {
  GetNextWaypoint(flecs::entity ent, const char* bb_name) {
    bb_ent = reg_entity_blackboard_var<flecs::entity>(ent, bb_name);
  }

  BehResult update(flecs::world&, flecs::entity, Blackboard& bb) override {
    auto prev_bb = bb.get<flecs::entity>(bb_ent);
    bb.set<flecs::entity>(bb_ent, prev_bb.get<Waypoint>()->next);
    return BEH_SUCCESS;
  }

  size_t bb_ent = size_t(-1);
};

struct Flee : public BehNode
{
  size_t entityBb = size_t(-1);
  Flee(flecs::entity entity, const char *bb_name)
  {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_RUNNING;
    entity.set([&](Action &a, const Position &pos)
    {
      flecs::entity targetEntity = bb.get<flecs::entity>(entityBb);
      if (!targetEntity.is_alive())
      {
        res = BEH_FAIL;
        return;
      }
      targetEntity.get([&](const Position &target_pos)
      {
        a.action = inverse_move(move_towards(pos, target_pos));
      });
    });
    return res;
  }
};

struct Patrol : public BehNode
{
  size_t pposBb = size_t(-1);
  float patrolDist = 1.f;
  Patrol(flecs::entity entity, float patrol_dist, const char *bb_name)
    : patrolDist(patrol_dist)
  {
    pposBb = reg_entity_blackboard_var<Position>(entity, bb_name);
    entity.set([&](Blackboard &bb, const Position &pos)
    {
      bb.set<Position>(pposBb, pos);
    });
  }

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_RUNNING;
    entity.set([&](Action &a, const Position &pos)
    {
      Position patrolPos = bb.get<Position>(pposBb);
      if (dist(pos, patrolPos) > patrolDist)
        a.action = move_towards(pos, patrolPos);
      else
        a.action = GetRandomValue(EA_MOVE_START, EA_MOVE_END - 1); // do a random walk
    });
    return res;
  }
};


BehNode *sequence(const std::vector<BehNode*> &nodes)
{
  Sequence *seq = new Sequence;
  for (BehNode *node : nodes)
    seq->pushNode(node);
  return seq;
}

BehNode *selector(const std::vector<BehNode*> &nodes)
{
  Selector *sel = new Selector;
  for (BehNode *node : nodes)
    sel->pushNode(node);
  return sel;
}

BehNode *move_to_entity(flecs::entity entity, const char *bb_name)
{
  return new MoveToEntity(entity, bb_name);
}

BehNode *is_low_hp(float thres)
{
  return new IsLowHp(thres);
}

BehNode *find_enemy(flecs::entity entity, float dist, const char *bb_name)
{
  return new FindEnemy(entity, dist, bb_name);
}

BehNode *flee(flecs::entity entity, const char *bb_name)
{
  return new Flee(entity, bb_name);
}

BehNode *patrol(flecs::entity entity, float patrol_dist, const char *bb_name)
{
  return new Patrol(entity, patrol_dist, bb_name);
}

BehNode* find_buff(flecs::entity entity, const char* bb_name) {
  return new FindBuff(entity, bb_name);
}

BehNode* get_next_waypoint(flecs::entity entity, const char* bb_name) {
  return new GetNextWaypoint(entity, bb_name);
}