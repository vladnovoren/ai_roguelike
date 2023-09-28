#pragma once

#include "stateMachine.h"

class AttackEnemyState : public State {
 public:
  void enter() override;
  void exit() override;
  void act(float /* dt*/, flecs::world & /*ecs*/,
           flecs::entity /*entity*/) override;
};

class MoveToEnemyState : public State {
 public:
  void enter() override;
  void exit() override;
  void act(float /* dt*/, flecs::world &ecs, flecs::entity entity) override;
};

class MoveToEntityState : public State {
 public:
  explicit MoveToEntityState(flecs::entity target);

  void enter() override;
  void exit() override;
  void act(float /* dt*/, flecs::world &ecs, flecs::entity actor) override;

 private:
  flecs::entity target_;
};

class HealEntityState : public State {
 public:
  explicit HealEntityState(flecs::entity target);

  void enter() override;
  void exit() override;
  void act(float /* dt*/, flecs::world &ecs, flecs::entity entity) override;

 private:
  flecs::entity target_;
};

class FleeFromEnemyState : public State {
 public:
  void enter() override;
  void exit() override;
  void act(float /* dt*/, flecs::world &ecs, flecs::entity entity) override;
};

class PatrolState : public State {
 public:
  explicit PatrolState(float dist);
  void enter() override;
  void exit() override;
  void act(float /* dt*/, flecs::world &ecs, flecs::entity entity) override;

 private:
  float patrolDist;
};

class NopState : public State {
 public:
  void enter() override;
  void exit() override;
  void act(float /* dt*/, flecs::world &ecs, flecs::entity entity) override;
};

class CraftHealState : public State {
 public:
  void enter() override;
  void exit() override;
  void act(float /* dt*/, flecs::world &ecs, flecs::entity entity) override;
};

class TrueTransition : public StateTransition {
 public:
  bool isAvailable(flecs::world &, flecs::entity) override;

  [[nodiscard]] std::unique_ptr<StateTransition> Copy() const override;
};

class EnemyAvailableTransition : public StateTransition {
 public:
  explicit EnemyAvailableTransition(float in_dist);

  bool isAvailable(flecs::world &ecs, flecs::entity e_pos) override;

  [[nodiscard]] std::unique_ptr<StateTransition> Copy() const override;

 public:
  float triggerDist;
};

class EntityNearTransition : public StateTransition {
 public:
  EntityNearTransition(flecs::entity target, float thres_dist);

  bool isAvailable(flecs::world &, flecs::entity actor) override;

  [[nodiscard]] std::unique_ptr<StateTransition> Copy() const override;

 private:
  flecs::entity target_;
  float thres_dist_;
};

class EntityLowHpTransition : public StateTransition {
 public:
  EntityLowHpTransition(flecs::entity entity, float thres);

  bool isAvailable(flecs::world &, flecs::entity) override;

  [[nodiscard]] std::unique_ptr<StateTransition> Copy() const override;

 private:
  flecs::entity entity_;
  float thres_;
};

class HealCraftedTransition : public StateTransition {
 public:
  explicit HealCraftedTransition(int heal_to_craft);

  bool isAvailable(flecs::world &, flecs::entity) override;

  [[nodiscard]] std::unique_ptr<StateTransition> Copy() const override;

 private:
  int heal_to_craft_ = 0;
};

class HealsPlantedTransition : public StateTransition {
 public:
  bool isAvailable(flecs::world &, flecs::entity) override;

  [[nodiscard]] std::unique_ptr<StateTransition> Copy() const override;
};

class Timer {
 public:
  void Start(time_t timeout);
  [[nodiscard]] bool IsDown() const;
  [[nodiscard]] bool IsStarted() const;
 private:
  bool started_ = false;
  time_t start_time_ = 0.f;
  time_t timeout_ = 0.f;
};

class TimeoutTransition : public StateTransition{
 public:
  explicit TimeoutTransition(time_t timeout);

  bool isAvailable(flecs::world &, flecs::entity) override;

  [[nodiscard]] std::unique_ptr<StateTransition> Copy() const override;
 private:
  Timer timer_;
  time_t timeout_;
};