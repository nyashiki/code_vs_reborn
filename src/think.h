#ifndef THINK_H_
#define THINK_H_

#include "game.h"
#include "action.h"

namespace Think {

void Init();
Action Start(const Game& game);

}  // Think

#endif  // THINK_H_
