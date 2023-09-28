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

class TrueTransition : public StateTransition {
 public:
  bool isAvailable(flecs::world &, flecs::entity) const override;

  [[nodiscard]] std::unique_ptr<StateTransition> Copy() const override;
};

class EnemyAvailableTransition : public StateTransition {
 public:
  explicit EnemyAvailableTransition(float in_dist);

  bool isAvailable(flecs::world &ecs, flecs::entity e_pos) const override;

  [[nodiscard]] std::unique_ptr<StateTransition> Copy() const override;

 public:
  float triggerDist;
};

class EntityNearTransition : public StateTransition {
 public:
  EntityNearTransition(flecs::entity target, float thres_dist);

  bool isAvailable(flecs::world &, flecs::entity actor) const override;

  [[nodiscard]] std::unique_ptr<StateTransition> Copy() const override;

 private:
  flecs::entity target_;
  float thres_dist_;
};

class EntityLowHpTransition : public StateTransition {
 public:
  EntityLowHpTransition(flecs::entity entity, float thres);

  bool isAvailable(flecs::world &, flecs::entity) const override;

  [[nodiscard]] std::unique_ptr<StateTransition> Copy() const override;

 private:
  flecs::entity entity_;
  float thres_;
};
