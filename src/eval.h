#ifndef EVAL_H_
#define EVAL_H_

#include "game.h"
#include "position.h"
#include "score.h"
#include "types.h"

namespace Eval {

/**
 * 周囲に空きがあるセルを一つ消し、得点を計算する。
 * それらの得点の最大得点を返す。
 */
Score EraseOne(const Position& current_position, bool ignore_bottom = false, Point* erase_point = nullptr, int* erase_number = nullptr, int target_column = -1);

}  // namespace Eval

#endif  // EVAL_H_
