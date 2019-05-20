#ifndef ACTION_H_
#define ACTION_H_

#include <string>

enum ActionType {
  NO_ACTION_TYPE, NORMAL, SKILL, RESIGN
};

struct Action {
  ActionType action_type;

  int column;
  int rotate;

  Action(): action_type(NO_ACTION_TYPE) { }
  Action(ActionType at, int c = -1, int r= -1);

  std::string ToString() const;

  bool operator==(const Action& action) const;
};


#endif  // ACTION_H_
