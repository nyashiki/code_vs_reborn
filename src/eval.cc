#include "eval.h"

#include <random>

Score Eval::EraseOne(const Position& current_position, bool ignore_bottom, Point* erase_point, int* erase_number, int target_column) {
  Score score_max = Score();

  for (int y = 4; y < kDANGER_HEIGHT; y++) {
    if (current_position.GetPackedCells(y) == 0ULL) {
      continue;
    }

    if (ignore_bottom && y >= kDANGER_HEIGHT - 2) {
      continue;
    }

    for (int x = 0; x < kWIDTH; x++) {
      if (target_column != -1 && x != target_column) {
        continue;
      }

      const int dy[4] = { -1, -1,  0, 0 };
      const int dx[4] = { -1,  1, -1, 1 };

      int target_cell = current_position.Get(y, x);
      if (target_cell == 0 || target_cell == 11) {
        continue;
      }

      // 上に何もないブロックを消さない
      if (current_position.Get(y - 1, x) == 0ULL) {
        continue;
      }

      // 左下と右下に何もないブロックを消さない
      if (y > 0) {
        if ((x == 0 || current_position.Get(y + 1, x - 1) == 0ULL) &&
            (x == kWIDTH - 1 || current_position.Get(y + 1, x + 1) == 0ULL)) {
          continue;
        }
      }

      bool is_adjacent_empty = false;
      for (int i = 0; i < 4; i++) {
        int ny = y + dy[i];
        int nx = x + dx[i];
        if (ny < 0 || ny >= kDANGER_HEIGHT || nx < 0 || nx >= kWIDTH) {
          continue;
        }

        if (current_position.Get(ny, nx) == 0) {
          is_adjacent_empty = true;
          break;
        }
      }

      if (!is_adjacent_empty) {
        continue;
      }

      Position position = current_position;

      int erase_num = position.Get(y, x);
      position.Set(y, x, 0);

      Score score = position.Simulate(Pack(), Action(NO_ACTION_TYPE));
      if (score.GetScoreSum() > score_max.GetScoreSum()) {
        score_max = score;

        if (erase_point != nullptr) {
          *erase_point = Point(y, x);
        }
        if (erase_number != nullptr) {
          *erase_number = erase_num;
        }
      }
    }
  }

  return score_max;
}
