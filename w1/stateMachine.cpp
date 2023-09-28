#include "stateMachine.h"

StateMachine::~StateMachine() {
  for (State *state : states) delete state;
}

void StateMachine::act(float dt, flecs::world &ecs, flecs::entity entity) {
  if (curStateIdx < states.size()) {
    for (const auto &transition :
         transitions[curStateIdx])
      if (transition.first.Get().isAvailable(ecs, entity)) {
        states[curStateIdx]->exit();
        curStateIdx = transition.second;
        states[curStateIdx]->enter();
        break;
      }
    states[curStateIdx]->act(dt, ecs, entity);
  } else
    curStateIdx = 0;
}

int StateMachine::addState(State *st) {
  int idx = states.size();
  states.push_back(st);
  transitions.emplace_back();
  return idx;
}

void StateMachine::addTransition(TransitionHandle trans, int from, int to) {
  transitions[from].emplace_back(std::move(trans), to);
}
