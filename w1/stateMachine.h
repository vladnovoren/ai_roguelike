#pragma once
#include <flecs.h>

#include <memory>
#include <vector>

class State {
 public:
  virtual void enter() const = 0;
  virtual void exit() const = 0;
  virtual void act(float dt, flecs::world &ecs, flecs::entity entity) const = 0;

  virtual ~State() = default;
};

class StateTransition;

StateTransition *create_enemy_available_transition(float dist);
StateTransition *create_enemy_reachable_transition();
StateTransition *create_hitpoints_less_than_transition(float thres);
StateTransition *create_negate_transition(StateTransition *in);
StateTransition *create_and_transition(StateTransition *lhs,
                                       StateTransition *rhs);
StateTransition *create_or_transition(StateTransition *lhs,
                                      StateTransition *rhs);
StateTransition *create_player_low_hp_transition(flecs::world &ecs,
                                                 float thres);
StateTransition *create_player_near_transition(flecs::world &ecs, float dist);

class StateTransition {
 public:
  virtual ~StateTransition() = default;
  virtual bool isAvailable(flecs::world &ecs, flecs::entity entity) const = 0;
  [[nodiscard]] virtual std::unique_ptr<StateTransition> Copy() const = 0;
};

class NegateTransition : public StateTransition {
  std::unique_ptr<const StateTransition> trans_impl_;

 public:
  explicit NegateTransition(const StateTransition &trans_impl)
      : trans_impl_(trans_impl.Copy()) {}

  NegateTransition(const NegateTransition &other_copy) {
    trans_impl_ = other_copy.trans_impl_->Copy();
  }

  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override {
    return !trans_impl_->isAvailable(ecs, entity);
  }

  [[nodiscard]] std::unique_ptr<StateTransition> Copy() const override {
    return std::make_unique<NegateTransition>(*this);
  }
};

class AndTransition : public StateTransition {
  std::unique_ptr<const StateTransition> lhs_;
  std::unique_ptr<const StateTransition> rhs_;

 public:
  AndTransition(const StateTransition &lhs, const StateTransition &rhs)
      : lhs_(lhs.Copy()), rhs_(rhs.Copy()) {}

  AndTransition(const AndTransition &other_copy)
      : lhs_(other_copy.lhs_->Copy()), rhs_(other_copy.rhs_->Copy()) {}

  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override {
    return lhs_->isAvailable(ecs, entity) && rhs_->isAvailable(ecs, entity);
  }

  [[nodiscard]] std::unique_ptr<StateTransition> Copy() const override {
    return std::make_unique<AndTransition>(*this);
  }
};

class OrTransition : public StateTransition {
  std::unique_ptr<const StateTransition> lhs_;
  std::unique_ptr<const StateTransition> rhs_;

 public:
  OrTransition(const StateTransition &lhs, const StateTransition &rhs)
      : lhs_(lhs.Copy()), rhs_(rhs.Copy()) {}

  OrTransition(const OrTransition &other_copy)
      : lhs_(other_copy.lhs_->Copy()), rhs_(other_copy.rhs_->Copy()) {}

  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override {
    return lhs_->isAvailable(ecs, entity) && rhs_->isAvailable(ecs, entity);
  }

  [[nodiscard]] std::unique_ptr<StateTransition> Copy() const override {
    return std::make_unique<OrTransition>(*this);
  }
};

class TransitionHandle {
 public:
  TransitionHandle() = delete;

  explicit TransitionHandle(std::unique_ptr<StateTransition> trans_impl)
      : trans_impl_(std::move(trans_impl)) {}

  template <typename T, typename... Args>
  static TransitionHandle Create(Args &&...args) {
    static_assert(std::is_base_of_v<StateTransition, T>);

    return TransitionHandle(
        std::unique_ptr<StateTransition>(new T(std::forward<Args>(args)...)));
  }

  TransitionHandle(const TransitionHandle &other_copy)
      : trans_impl_(other_copy.trans_impl_->Copy()) {}

  TransitionHandle(TransitionHandle &&other_move) noexcept
      : trans_impl_(std::move(other_move.trans_impl_)) {}

  TransitionHandle &operator=(const TransitionHandle &other_copy) {
    TransitionHandle tmp(other_copy);
    std::swap(*this, tmp);
    return *this;
  }

  TransitionHandle &operator=(TransitionHandle &&other_move) noexcept {
    trans_impl_ = std::move(other_move.trans_impl_);
    return *this;
  }
 public:
  [[nodiscard]] const StateTransition& Get() const {
    return *trans_impl_;
  }

 public:
  TransitionHandle operator!() const {
    return Create<NegateTransition>(*trans_impl_);
  }

  TransitionHandle operator&&(const TransitionHandle &rhs) const {
    return Create<AndTransition>(*trans_impl_, *rhs.trans_impl_);
  }

  TransitionHandle operator||(const TransitionHandle &rhs) const {
    return Create<OrTransition>(*trans_impl_, *rhs.trans_impl_);
  }

 private:
  std::unique_ptr<StateTransition> trans_impl_;
};

class StateMachine {
  int curStateIdx = 0;
  std::vector<State *> states;
  std::vector<std::vector<std::pair<TransitionHandle, int>>> transitions;

 public:
  StateMachine() = default;
  StateMachine(const StateMachine &sm) = default;
  StateMachine(StateMachine &&sm) = default;

  ~StateMachine();

  StateMachine &operator=(const StateMachine &sm) = default;
  StateMachine &operator=(StateMachine &&sm) = default;

  void act(float dt, flecs::world &ecs, flecs::entity entity);

  int addState(State *st);
  void addTransition(TransitionHandle trans, int from, int to);
};
