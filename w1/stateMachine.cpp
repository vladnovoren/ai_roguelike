#include "stateMachine.h"

void StateMachine::enter() {}

void StateMachine::act(float dt, flecs::world &ecs, flecs::entity entity) {
  if (cur_state_id_ < states_.size()) {
    for (const auto &transition : transitions_[cur_state_id_])
      if (transition.first.Get().isAvailable(ecs, entity)) {
        states_[cur_state_id_]->exit();
        cur_state_id_ = transition.second;
        states_[cur_state_id_]->enter();
        break;
      }
    states_[cur_state_id_]->act(dt, ecs, entity);
  } else
    cur_state_id_ = 0;
}

void StateMachine::exit() {}

void StateMachine::addTransition(TransitionHandle trans, int from, int to) {
  transitions_[from].emplace_back(std::move(trans), to);
}
