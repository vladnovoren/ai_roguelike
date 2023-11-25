#include "aiLibrary.h"
#include "aiUtils.h"
#include "blackboard.h"
#include "ecsTypes.h"
#include "math.h"
#include "raylib.h"

struct CompoundNode : public BehNode {
  std::vector<BehNode *> nodes;

  virtual ~CompoundNode() {
    for (BehNode *node : nodes) delete node;
    nodes.clear();
  }

  CompoundNode &pushNode(BehNode *node) {
    nodes.push_back(node);
    return *this;
  }
};

struct Sequence : public CompoundNode {
  BehResult update(flecs::world &ecs, flecs::entity entity,
                   Blackboard &bb) override {
    for (BehNode *node : nodes) {
      BehResult res = node->update(ecs, entity, bb);
      if (res != BEH_SUCCESS) return res;
    }
    return BEH_SUCCESS;
  }
};

struct Selector : public CompoundNode {
  BehResult update(flecs::world &ecs, flecs::entity entity,
                   Blackboard &bb) override {
    for (BehNode *node : nodes) {
      BehResult res = node->update(ecs, entity, bb);
      if (res != BEH_FAIL) return res;
    }
    return BEH_FAIL;
  }
};

struct UtilitySelector : public BehNode {
  std::vector<std::pair<BehNode *, utility_function>> utilityNodes;

  BehResult update(flecs::world &ecs, flecs::entity entity,
                   Blackboard &bb) override {
    std::vector<std::pair<float, size_t>> utilityScores;
    for (size_t i = 0; i < utilityNodes.size(); ++i) {
      const float utilityScore = utilityNodes[i].second(bb);
      utilityScores.push_back(std::make_pair(utilityScore, i));
    }
    std::sort(utilityScores.begin(), utilityScores.end(),
              [](auto &lhs, auto &rhs) { return lhs.first > rhs.first; });
    for (const std::pair<float, size_t> &node : utilityScores) {
      size_t nodeIdx = node.second;
      BehResult res = utilityNodes[nodeIdx].first->update(ecs, entity, bb);
      if (res != BEH_FAIL) return res;
    }
    return BEH_FAIL;
  }
};

struct RandomUtilitySelector : public UtilitySelector {
  BehResult update(flecs::world &ecs, flecs::entity ent,
                   Blackboard &bb) override {
    auto [scores, scores_sum] = CalcUtilityScores(bb);
    for (const auto &node : utilityNodes) {
      auto prob = get_random_float(0, scores_sum);
      size_t node_id = 0;
      for (; prob > 0.f; ++node_id) {
        prob -= scores[node_id];
      }
      --node_id;

      auto res = utilityNodes[node_id].first->update(ecs, ent, bb);
      if (res != BEH_FAIL) return res;

      scores_sum -= scores[node_id];
      scores[node_id] = 0.f;
    }
    return BEH_FAIL;
  }

 private:
  std::tuple<std::vector<float>, float> CalcUtilityScores(Blackboard &bb) {
    std::vector<float> scores;
    float sum = 0.f;
    for (const auto &node : utilityNodes) {
      auto score = node.second(bb);
      scores.push_back(score);
      sum += score;
    }
    return {scores, sum};
  }
};

struct InertialUtilitySelector : public UtilitySelector {
 public:
  explicit InertialUtilitySelector(std::vector<float> inertias)
      : inertias(std::move(inertias)) {}

 private:
  std::vector<std::pair<float, size_t>> GetSortedScores(Blackboard& bb) {
    std::vector<std::pair<float, size_t>> scores;
    for (size_t i = 0; i < utilityNodes.size(); ++i) {
      const float utilityScore = utilityNodes[i].second(bb) + inertias[i];
      scores.emplace_back(utilityScore, i);
    }
    std::sort(scores.begin(), scores.end(),
              [](auto &lhs, auto &rhs) { return lhs.first > rhs.first; });
    return scores;
  }

 public:
  BehResult update(flecs::world &ecs, flecs::entity entity,
                   Blackboard &bb) override {
    auto scores = GetSortedScores(bb);
    for (const auto &node : scores) {
      size_t node_id = node.second;
      auto res = utilityNodes[node_id].first->update(ecs, entity, bb);
      if (res != BEH_FAIL) {
        UpdateInertia(node_id);
        return res;
      }
    }
    return BEH_FAIL;
  }

 private:
  void UpdateInertia(size_t node_id) {
    float prev = inertias[node_id];
    std::ranges::fill(inertias, 0);
    if (prev > 0)
      inertias[node_id] = prev - cooldown;
    else
      inertias[node_id] = prev + inertia_amount;
  }

 private:
  std::vector<float> inertias;

  float inertia_amount = 100.f;
  float cooldown = 10.f;
};

struct MoveToEntity : public BehNode {
  size_t entityBb = size_t(-1);  // wraps to 0xff...
  MoveToEntity(flecs::entity entity, const char *bb_name) {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }

  BehResult update(flecs::world &, flecs::entity entity,
                   Blackboard &bb) override {
    BehResult res = BEH_RUNNING;
    entity.set([&](Action &a, const Position &pos) {
      flecs::entity targetEntity = bb.get<flecs::entity>(entityBb);
      if (!targetEntity.is_alive()) {
        res = BEH_FAIL;
        return;
      }
      targetEntity.get([&](const Position &target_pos) {
        if (pos != target_pos) {
          a.action = move_towards(pos, target_pos);
          res = BEH_RUNNING;
        } else
          res = BEH_SUCCESS;
      });
    });
    return res;
  }
};

struct RandomMove : public BehNode {
  BehResult update(flecs::world &, flecs::entity ent, Blackboard &) override {
    ent.set([](Action &action, const Position &position) {
      action.action = GetRandomValue(EA_MOVE_START, EA_MOVE_END - 1);
    });
    return BEH_RUNNING;
  }
};

struct IsLowHp : public BehNode {
  float threshold = 0.f;
  IsLowHp(float thres) : threshold(thres) {}

  BehResult update(flecs::world &, flecs::entity entity,
                   Blackboard &) override {
    BehResult res = BEH_SUCCESS;
    entity.get([&](const Hitpoints &hp) {
      res = hp.hitpoints < threshold ? BEH_SUCCESS : BEH_FAIL;
    });
    return res;
  }
};

struct FindEnemy : public BehNode {
  size_t entityBb = size_t(-1);
  float distance = 0;
  FindEnemy(flecs::entity entity, float in_dist, const char *bb_name)
      : distance(in_dist) {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }
  BehResult update(flecs::world &ecs, flecs::entity entity,
                   Blackboard &bb) override {
    BehResult res = BEH_FAIL;
    static auto enemiesQuery = ecs.query<const Position, const Team>();
    entity.set([&](const Position &pos, const Team &t) {
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
      if (ecs.is_valid(closestEnemy) && closestDist <= distance) {
        bb.set<flecs::entity>(entityBb, closestEnemy);
        res = BEH_SUCCESS;
      }
    });
    return res;
  }
};

struct Flee : public BehNode {
  size_t entityBb = size_t(-1);
  Flee(flecs::entity entity, const char *bb_name) {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }

  BehResult update(flecs::world &, flecs::entity entity,
                   Blackboard &bb) override {
    BehResult res = BEH_RUNNING;
    entity.set([&](Action &a, const Position &pos) {
      flecs::entity targetEntity = bb.get<flecs::entity>(entityBb);
      if (!targetEntity.is_alive()) {
        res = BEH_FAIL;
        return;
      }
      targetEntity.get([&](const Position &target_pos) {
        a.action = inverse_move(move_towards(pos, target_pos));
      });
    });
    return res;
  }
};

struct Patrol : public BehNode {
  size_t pposBb = size_t(-1);
  float patrolDist = 1.f;
  Patrol(flecs::entity entity, float patrol_dist, const char *bb_name)
      : patrolDist(patrol_dist) {
    pposBb = reg_entity_blackboard_var<Position>(entity, bb_name);
    entity.set([&](Blackboard &bb, const Position &pos) {
      bb.set<Position>(pposBb, pos);
    });
  }

  BehResult update(flecs::world &, flecs::entity entity,
                   Blackboard &bb) override {
    BehResult res = BEH_RUNNING;
    entity.set([&](Action &a, const Position &pos) {
      Position patrolPos = bb.get<Position>(pposBb);
      if (dist(pos, patrolPos) > patrolDist)
        a.action = move_towards(pos, patrolPos);
      else
        a.action =
            GetRandomValue(EA_MOVE_START, EA_MOVE_END - 1);  // do a random walk
    });
    return res;
  }
};

struct PatchUp : public BehNode {
  float hpThreshold = 100.f;
  PatchUp(float threshold) : hpThreshold(threshold) {}

  BehResult update(flecs::world &, flecs::entity entity,
                   Blackboard &) override {
    BehResult res = BEH_SUCCESS;
    entity.set([&](Action &a, Hitpoints &hp) {
      if (hp.hitpoints >= hpThreshold) return;
      res = BEH_RUNNING;
      a.action = EA_HEAL_SELF;
    });
    return res;
  }
};

BehNode *sequence(const std::vector<BehNode *> &nodes) {
  Sequence *seq = new Sequence;
  for (BehNode *node : nodes) seq->pushNode(node);
  return seq;
}

BehNode *selector(const std::vector<BehNode *> &nodes) {
  Selector *sel = new Selector;
  for (BehNode *node : nodes) sel->pushNode(node);
  return sel;
}

BehNode *utility_selector(
    const std::vector<std::pair<BehNode *, utility_function>> &nodes) {
  UtilitySelector *usel = new UtilitySelector;
  usel->utilityNodes = std::move(nodes);
  return usel;
}

BehNode *random_utility_selector(
    const std::vector<std::pair<BehNode *, utility_function>> &nodes) {
  auto selector = new RandomUtilitySelector;
  selector->utilityNodes = nodes;
  return selector;
}

BehNode *inertial_utility_selector(
    const std::vector<std::pair<BehNode *, utility_function>> &nodes) {
  std::vector<float> inertia(nodes.size(), 0.f);
  auto selector = new InertialUtilitySelector(inertia);
  selector->utilityNodes = nodes;
  return selector;
}

BehNode *move_to_entity(flecs::entity entity, const char *bb_name) {
  return new MoveToEntity(entity, bb_name);
}

BehNode *random_move() { return new RandomMove(); }

BehNode *is_low_hp(float thres) { return new IsLowHp(thres); }

BehNode *find_enemy(flecs::entity entity, float dist, const char *bb_name) {
  return new FindEnemy(entity, dist, bb_name);
}

BehNode *flee(flecs::entity entity, const char *bb_name) {
  return new Flee(entity, bb_name);
}

BehNode *patrol(flecs::entity entity, float patrol_dist, const char *bb_name) {
  return new Patrol(entity, patrol_dist, bb_name);
}

BehNode *patch_up(float thres) { return new PatchUp(thres); }
