#include "roguelike.h"

#include "aiLibrary.h"
#include "ecsTypes.h"
#include "raylib.h"
#include "stateMachine.h"

static void add_patrol_attack_flee_sm(flecs::entity entity) {
  entity.get([&](StateMachine &sm) {
    int patrol = sm.addState<PatrolState>(3.f);
    int moveToEnemy = sm.addState<MoveToEnemyState>();
    int fleeFromEnemy = sm.addState<FleeFromEnemyState>();

    auto enemy_available_3 =
        TransitionHandle::Create<EnemyAvailableTransition>(3.f);
    auto enemy_available_5 =
        TransitionHandle::Create<EnemyAvailableTransition>(5.f);
    auto enemy_available_7 =
        TransitionHandle::Create<EnemyAvailableTransition>(7.f);
    auto hitpoints_less =
        TransitionHandle::Create<EntityLowHpTransition>(entity, 60.f);

    sm.addTransition(enemy_available_3, patrol, moveToEnemy);
    sm.addTransition(!enemy_available_5, moveToEnemy, patrol);
    sm.addTransition(hitpoints_less && enemy_available_5, moveToEnemy,
                     fleeFromEnemy);
    sm.addTransition(hitpoints_less && enemy_available_3, patrol,
                     fleeFromEnemy);
    sm.addTransition(!enemy_available_7, fleeFromEnemy, patrol);
  });
}

static void add_patrol_flee_sm(flecs::entity entity) {
  entity.get([](StateMachine &sm) {
    int patrol = sm.addState<PatrolState>(3.f);
    int fleeFromEnemy = sm.addState<FleeFromEnemyState>();

    sm.addTransition(TransitionHandle::Create<EnemyAvailableTransition>(3.f),
                     patrol, fleeFromEnemy);
    sm.addTransition(!TransitionHandle::Create<EnemyAvailableTransition>(5.f),
                     fleeFromEnemy, patrol);
  });
}

static void add_attack_sm(flecs::entity entity) {
  entity.get([](StateMachine &sm) { sm.addState<MoveToEnemyState>(); });
}

static void AddBerzerkStateMachine(flecs::entity entity) {
  entity.get([](StateMachine &sm) {
    auto patrol = sm.addState<PatrolState>(3.f);
    auto move_to_enemy = sm.addState<MoveToEnemyState>();

    auto enemy_available =
        TransitionHandle::Create<EnemyAvailableTransition>(5.f);

    sm.addTransition(enemy_available, patrol, move_to_enemy);
    sm.addTransition(!enemy_available, move_to_enemy, patrol);
  });
}

static void AddHealingMonsterStateMachine(flecs::entity entity,
                                          float hp_thres) {
  entity.get([&](StateMachine &sm) {
    auto patrol = sm.addState<PatrolState>(3.f);
    auto move_to_enemy = sm.addState<MoveToEnemyState>();
    auto self_heal = sm.addState<HealEntityState>(entity);

    auto enemy_near = TransitionHandle::Create<EnemyAvailableTransition>(5.f);
    auto low_hp =
        TransitionHandle::Create<EntityLowHpTransition>(entity, hp_thres);

    sm.addTransition(enemy_near, patrol, move_to_enemy);
    sm.addTransition(!enemy_near, move_to_enemy, patrol);

    sm.addTransition(low_hp, patrol, self_heal);
    sm.addTransition(!enemy_near, self_heal, patrol);

    sm.addTransition(enemy_near, self_heal, move_to_enemy);
    sm.addTransition(low_hp, move_to_enemy, self_heal);
  });
}

static void AddSwordsmanHealerStateMachine(flecs::entity entity,
                                           flecs::entity target,
                                           float hp_thres) {
  entity.get([&](StateMachine &sm) {
    auto move_to_target = sm.addState<MoveToEntityState>(target);
    auto move_to_enemy = sm.addState<MoveToEnemyState>();
    auto heal = sm.addState<HealEntityState>(target);

    auto enemy_near = TransitionHandle::Create<EnemyAvailableTransition>(5.f);
    auto target_near =
        TransitionHandle::Create<EntityNearTransition>(target, 3.f);
    auto low_hp =
        TransitionHandle::Create<EntityLowHpTransition>(target, hp_thres);
    auto true_trans = TransitionHandle::Create<TrueTransition>();

    sm.addTransition(enemy_near && !low_hp, move_to_target, move_to_enemy);
    sm.addTransition((low_hp && !target_near) || !enemy_near, move_to_enemy,
                     move_to_target);

    sm.addTransition(target_near && low_hp, move_to_target, heal);
    sm.addTransition(true_trans, heal, move_to_target);

    sm.addTransition(enemy_near, heal, move_to_enemy);
    sm.addTransition(target_near && low_hp, move_to_enemy, heal);
  });
}

static flecs::entity create_monster(flecs::world &ecs, int x, int y,
                                    Color color) {
  return ecs.entity()
      .set(Position{x, y})
      .set(MovePos{x, y})
      .set(PatrolPos{x, y})
      .set(Hitpoints{100.f})
      .set(Action{EA_NOP})
      .set(Color{color})
      .set(StateMachine{})
      .set(Team{1})
      .set(NumActions{1, 0})
      .set(MeleeDamage{20.f});
}

static flecs::entity create_heal_monster(flecs::world &ecs, int x, int y,
                                         Color color) {
  return ecs.entity()
      .set(Position{x, y})
      .set(MovePos{x, y})
      .set(PatrolPos{x, y})
      .set(Hitpoints{100.f})
      .set(Action{EA_NOP})
      .set(Color{color})
      .set(StateMachine())
      .set(Team{1})
      .set(NumActions{1, 0})
      .set(MeleeDamage{20.f})
      .set(HealerPoints(50.f));
}

static void create_player(flecs::world &ecs, int x, int y) {
  ecs.entity("player")
      .set(Position{x, y})
      .set(MovePos{x, y})
      .set(Hitpoints{100.f})
      .set(GetColor(0xeeeeeeff))
      .set(Action{EA_NOP})
      .add<IsPlayer>()
      .set(Team{0})
      .set(PlayerInput{})
      .set(NumActions{2, 0})
      .set(MeleeDamage{50.f});
}

static flecs::entity create_heal_swordsman(flecs::world &ecs, int x, int y,
                                           Color color) {
  return ecs.entity()
      .set(Position{x, y})
      .set(MovePos{x, y})
      .set(Hitpoints{100.f})
      .set(Action{EA_NOP})
      .set(Color{color})
      .set(StateMachine{})
      .set(Team{0})
      .set(NumActions{1, 0})
      .set(MeleeDamage{40.f})
      .set(HealerPoints(20.f));
}

static void create_heal(flecs::world &ecs, int x, int y, float amount) {
  ecs.entity()
      .set(Position{x, y})
      .set(HealAmount{amount})
      .set(GetColor(0x44ff44ff));
}

static void create_powerup(flecs::world &ecs, int x, int y, float amount) {
  ecs.entity()
      .set(Position{x, y})
      .set(PowerupAmount{amount})
      .set(Color{255, 255, 0, 255});
}

static void register_roguelike_systems(flecs::world &ecs) {
  ecs.system<PlayerInput, Action, const IsPlayer>().each(
      [&](PlayerInput &inp, Action &a, const IsPlayer) {
        bool left = IsKeyDown(KEY_LEFT);
        bool right = IsKeyDown(KEY_RIGHT);
        bool up = IsKeyDown(KEY_UP);
        bool down = IsKeyDown(KEY_DOWN);
        if (left && !inp.left) a.action = EA_MOVE_LEFT;
        if (right && !inp.right) a.action = EA_MOVE_RIGHT;
        if (up && !inp.up) a.action = EA_MOVE_UP;
        if (down && !inp.down) a.action = EA_MOVE_DOWN;
        inp.left = left;
        inp.right = right;
        inp.up = up;
        inp.down = down;
      });
  ecs.system<const Position, const Color>()
      .term<TextureSource>(flecs::Wildcard)
      .not_()
      .each([&](const Position &pos, const Color color) {
        const Rectangle rect = {float(pos.x), float(pos.y), 1, 1};
        DrawRectangleRec(rect, color);
      });
  ecs.system<const Position, const Color>()
      .term<TextureSource>(flecs::Wildcard)
      .each([&](flecs::entity e, const Position &pos, const Color color) {
        const auto textureSrc = e.target<TextureSource>();
        DrawTextureQuad(*textureSrc.get<Texture2D>(), Vector2{1, 1},
                        Vector2{0, 0},
                        Rectangle{float(pos.x), float(pos.y), 1, 1}, color);
      });
}

void init_roguelike(flecs::world &ecs) {
  register_roguelike_systems(ecs);

  auto player = ecs.lookup("player");

  AddBerzerkStateMachine(create_monster(ecs, 5, 5, GetColor(0xff0000ff)));
  AddHealingMonsterStateMachine(
      create_heal_monster(ecs, 10, -5, GetColor(0x0000ffff)), 30);
  AddSwordsmanHealerStateMachine(
      create_heal_swordsman(ecs, 10, 5, GetColor(0x00ff00ff)), player, 30);

  create_player(ecs, 0, 0);
  //
  //  create_powerup(ecs, 7, 7, 10.f);
  //  create_powerup(ecs, 10, -6, 10.f);
  //  create_powerup(ecs, 10, -4, 10.f);
  //
  //  create_heal(ecs, -5, -5, 50.f);
  //  create_heal(ecs, -5, 5, 50.f);
}

static bool is_player_acted(flecs::world &ecs) {
  static auto processPlayer = ecs.query<const IsPlayer, const Action>();
  bool playerActed = false;
  processPlayer.each([&](const IsPlayer, const Action &a) {
    playerActed = a.action != EA_NOP;
  });
  return playerActed;
}

static bool upd_player_actions_count(flecs::world &ecs) {
  static auto updPlayerActions = ecs.query<const IsPlayer, NumActions>();
  bool actionsReached = false;
  updPlayerActions.each([&](const IsPlayer, NumActions &na) {
    na.curActions = (na.curActions + 1) % na.numActions;
    actionsReached |= na.curActions == 0;
  });
  return actionsReached;
}

static Position move_pos(Position pos, int action) {
  if (action == EA_MOVE_LEFT)
    pos.x--;
  else if (action == EA_MOVE_RIGHT)
    pos.x++;
  else if (action == EA_MOVE_UP)
    pos.y--;
  else if (action == EA_MOVE_DOWN)
    pos.y++;
  return pos;
}

static void process_actions(flecs::world &ecs) {
  static auto processActions =
      ecs.query<Action, Position, MovePos, const MeleeDamage, const Team>();
  static auto checkAttacks = ecs.query<const MovePos, Hitpoints, const Team>();
  // Process all actions
  ecs.defer([&] {
    processActions.each([&](flecs::entity entity, Action &a, Position &pos,
                            MovePos &mpos, const MeleeDamage &dmg,
                            const Team &team) {
      Position nextPos = move_pos(pos, a.action);
      bool blocked = false;
      checkAttacks.each([&](flecs::entity enemy, const MovePos &epos,
                            Hitpoints &hp, const Team &enemy_team) {
        if (entity != enemy && epos == nextPos) {
          blocked = true;
          if (team.team != enemy_team.team) hp.hitpoints -= dmg.damage;
        }
      });
      if (blocked)
        a.action = EA_NOP;
      else
        mpos = nextPos;
    });
    // now move
    processActions.each([&](flecs::entity entity, Action &a, Position &pos,
                            MovePos &mpos, const MeleeDamage &, const Team &) {
      pos = mpos;
      a.action = EA_NOP;
    });
  });

  static auto deleteAllDead = ecs.query<const Hitpoints>();
  ecs.defer([&] {
    deleteAllDead.each([&](flecs::entity entity, const Hitpoints &hp) {
      if (hp.hitpoints <= 0.f) entity.destruct();
    });
  });

  static auto playerPickup =
      ecs.query<const IsPlayer, const Position, Hitpoints, MeleeDamage>();
  static auto healPickup = ecs.query<const Position, const HealAmount>();
  static auto powerupPickup = ecs.query<const Position, const PowerupAmount>();
  ecs.defer([&] {
    playerPickup.each([&](const IsPlayer &, const Position &pos, Hitpoints &hp,
                          MeleeDamage &dmg) {
      healPickup.each([&](flecs::entity entity, const Position &ppos,
                          const HealAmount &amt) {
        if (pos == ppos) {
          hp.hitpoints += amt.amount;
          entity.destruct();
        }
      });
      powerupPickup.each([&](flecs::entity entity, const Position &ppos,
                             const PowerupAmount &amt) {
        if (pos == ppos) {
          dmg.damage += amt.amount;
          entity.destruct();
        }
      });
    });
  });
}

void process_turn(flecs::world &ecs) {
  static auto stateMachineAct = ecs.query<StateMachine>();
  if (is_player_acted(ecs)) {
    if (upd_player_actions_count(ecs)) {
      // Plan action for NPCs
      ecs.defer([&] {
        stateMachineAct.each(
            [&](flecs::entity e, StateMachine &sm) { sm.act(0.f, ecs, e); });
      });
    }
    process_actions(ecs);
  }
}

void print_stats(flecs::world &ecs) {
  static auto playerStatsQuery =
      ecs.query<const IsPlayer, const Hitpoints, const MeleeDamage>();
  playerStatsQuery.each(
      [&](const IsPlayer &, const Hitpoints &hp, const MeleeDamage &dmg) {
        DrawText(TextFormat("hp: %d", int(hp.hitpoints)), 20, 20, 20, WHITE);
        DrawText(TextFormat("power: %d", int(dmg.damage)), 20, 40, 20, WHITE);
      });
}
