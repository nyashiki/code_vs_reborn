#include "action.h"

Action::Action(ActionType at, int c, int r):
  action_type(at), column(c), rotate(r) { }

std::string Action::ToString() const {
  if (action_type == SKILL) {
    return "S";
  } else if (action_type == RESIGN) {
    return "0 0";
  } else {
    return std::to_string(column) + " " + std::to_string(rotate);
  }
}

bool Action::operator==(const Action& action) const {
  if (this->action_type != action.action_type) {
    return false;
  }
  if (this->column != action.column) {
    return false;
  }
  if (this->rotate != action.rotate) {
    return false;
  }

  return true;
}
