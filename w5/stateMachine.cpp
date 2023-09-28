#include "stateMachine.h"

StateMachine::~StateMachine()
{
  for (State* state : states_)
    delete state;
  states_.clear();
  for (auto &transList : transitions_)
    for (auto &transition : transList)
      delete transition.first;
  transitions_.clear();
}

void StateMachine::act(float dt, flecs::world &ecs, flecs::entity entity)
{
  if (cur_state_id_ < states_.size())
  {
    for (const std::pair<StateTransition*, int> &transition :
         transitions_[cur_state_id_])
      if (transition.first->isAvailable(ecs, entity))
      {
        states_[cur_state_id_]->exit();
        cur_state_id_ = size_t(transition.second);
        states_[cur_state_id_]->enter();
        break;
      }
    states_[cur_state_id_]->act(dt, ecs, entity);
  }
  else
    cur_state_id_ = 0;
}

int StateMachine::addState(State *st)
{
  size_t idx = states_.size();
  states_.push_back(st);
  transitions_.push_back(std::vector<std::pair<StateTransition*, int>>());
  return int(idx);
}

void StateMachine::addTransition(StateTransition *trans, int from, int to)
{
  transitions_[size_t(from)].push_back(std::make_pair(trans, to));
}

