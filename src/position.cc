#include "position.h"
#include "types.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <immintrin.h>

namespace {


bool line_disappear_mask[0b11111111];  // 隣接横2ブロックが消えるかどうかのマスク
PackedCells other_disappear_mask[0b1111][0b111111111111];  // 左上、上、右上のマスク
const int chain_scores[64] = {
  0, 1, 2, 3, 6, 9, 12, 17, 23,
  32, 42, 56, 74, 97, 127, 167, 218,
  285, 371, 483, 630, 820, 1067, 1388, 1806,
  2348, 3054, 3971, 5164, 6714, 8729, 11349, 14755,
  19183, 24939, 32422, 42150, 54796, 71237, 92609, 120392,
  156511, 203466, 264507, 343860, 447019, 581126, 755465, 982105,
  1276738, 1659760, 2157689, 2804997, 3646498, 4740448, 6162584, 8011360,
  10414770, 13539202, 17600963, 22881253, 29745631, 38669321, 50270118
};

}  // namespace

void Position::Init() {
  /**
   * 予め消えるパターンのbitmaskを生成しておき、実際にシミュレーションする際には
   * ビット演算で行えるようにしておく。
   */

  // 左上、上、右上、右（とその対称）について調べておけば、それが全て
  memset(line_disappear_mask, 0, sizeof(line_disappear_mask));
  memset(other_disappear_mask, 0, sizeof(other_disappear_mask));

  // 横に関するマスク
  for (int x = 0; x < kWIDTH; x++) {
    const int nx = x + 1;
    if (nx < kWIDTH) {
      for (int num = 1; num <= 5; num++) {
        int num2 = 10 - num;  // 相方の数字

        PackedCells pattern = ((num << 4) | num2);
        PackedCells pattern2 = ((num2 << 4) | num);
        line_disappear_mask[pattern] = true;
        line_disappear_mask[pattern2] = true;
      }
    }
  }

  // 横以外に関するマスク
  {
    for (int num = 1; num <= 9; num++) {
      int op_num = 10 - num;
      for (int num1 = 0; num1 <= 11; num1++) {
        if (num1 == 10) {
          continue;
        }
        for (int num2 = 0; num2 <= 11; num2++) {
          if (num2 == 10) {
            continue;
          }
          for (int num3 = 0; num3 <= 11; num3++) {
            if (num3 == 10) {
              continue;
            }
            PackedCells target = (num1 << 8ULL) | (num2 << 4ULL) | (num3 << 0ULL);
            PackedCells mask = 0;
            if (num1 == op_num) { mask |= (op_num << 8ULL); }
            if (num2 == op_num) { mask |= (op_num << 4ULL); }
            if (num3 == op_num) { mask |= (op_num << 0ULL); }
            other_disappear_mask[num][target] = mask;
          }
        }
      }
   }
  }
}

Position::Position() {
  memset(cells, 0, sizeof(cells));
}

void Position::Print() const {
  for (int y = 0; y < kDANGER_HEIGHT; y++) {
    for (int x = 0; x < kWIDTH; x++) {
      fprintf(stderr, "%3" SCNuFAST64, Get(y, x));
    }
    fprintf(stderr, "\n");
  }
}

void Position::GetInput() {
  for (int y = 3; y < kDANGER_HEIGHT; y++) {
    PackedCells data_ = 0;
    for (int x = 0; x < kWIDTH; x++) {
      data_ <<= 4;

      PackedCells d = 0;
      std::cin >> d;
      data_ |= d;
    }
    cells[y] = data_;
  }

  std::string end_str;
  std::cin >> end_str;
  assert(end_str == "END");
}

uint_fast64_t Position::Get(int y, int x) const {
  int shift = 4 * (kWIDTH - 1 - x);

#ifdef SERVER
  return ((cells[y] & (0b1111ULL << shift)) >> shift);
#endif

  return _pext_u64(cells[y], (0b1111ULL << shift));
}

uint_fast64_t Position::GetPackedCells(int y) const {
  return cells[y];
}


void Position::Set(int y, int x, uint_fast64_t cell) {
  int shift = 4 * (kWIDTH - 1 - x);
  uint_fast64_t bitmask = ~(0b1111ULL << shift);
  cells[y] &= bitmask;  // 対象のcellを0にする
  cells[y] |= (cell << shift);  // 対象のcellに値を設定
}

Score Position::Simulate(const Pack& pack, const Action& action) {
  // デバッグ用 現在の状態を保存しておく
  // Position start_position = *this;

  Score score;

  if (action.action_type == SKILL) {
    // 5のブロック及びその周囲8升のお邪魔でないブロックを消す
    const int dy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
    const int dx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };

    int block_count = 0;
    PackedCells disappear_cells[kDANGER_HEIGHT] = { };
    bool disappear[kDANGER_HEIGHT][kWIDTH] = { };

    for (int y = 0; y < kDANGER_HEIGHT; y++) {
      for (int x = 0; x < kWIDTH; x++) {
        if (Get(y, x) == 5) {
          int shift = 4 * (kWIDTH - 1 - x);
          if (!disappear[y][x]) {
            block_count++;
          }
          disappear[y][x] = true;
          disappear_cells[y] |= (5ULL << shift);
          for (int i = 0; i < 8; i++) {
            int ny = y + dy[i];
            int nx = x + dx[i];

            if (ny < 0 || ny >= kDANGER_HEIGHT || nx < 0 || nx >= kWIDTH) {
              continue;
            }
            if (disappear[ny][nx]) {
              continue;
            }
            int target_value = Get(ny, nx);
            if (target_value == 0 || target_value == 11) {
              continue;
            }

            int n_shift = 4 * (kWIDTH - 1 - nx);
            disappear_cells[ny] |= ((PackedCells)target_value << n_shift);
            disappear[ny][nx] = true;
            block_count++;
          }
        }
      }
    }

    for (int y = 0; y < kDANGER_HEIGHT; y++) {
      cells[y] ^= disappear_cells[y];
    }

    score.explosion_score += floor(25 * pow(2.0, block_count / 12.0));
    if (block_count == 0) {
      score.explosion_score = 0;
    }

  } else if (action.action_type == NORMAL) {
    // 列ごとに独立に扱ってよい
    Pack rotate_pack = pack.GetRotated(action.rotate);

    // デンジャーラインにブロックが存在することはない
    // それゆえ、一度デンジャーライン上に置いて、ブロックを落とせば良い
    cells[0] |= (rotate_pack.GetTopLine() << (4 * (kWIDTH - 2 - action.column)));
    cells[1] |= (rotate_pack.GetBottomLine() << (4 * (kWIDTH - 2 - action.column)));
  }

  int chains = 0;
  while (true) {
    bool update = false;

    // ブロックを落とす
    {
      PackedCells disappear_cells[kDANGER_HEIGHT] = { };
      PackedCells appear_cells[kDANGER_HEIGHT] = { };

      for (int x = 0; x < kWIDTH; x++) {
        // 下から探して、一番初めに空であるy座標を探す
        int ground_y = kDANGER_HEIGHT - 1;
        for (; ground_y >= 0; ground_y--) {
          int shift = 4 * (kWIDTH - 1 - x);
          PackedCells target_cell = (cells[ground_y] & (0b1111ULL << shift));
          if (target_cell == 0) {
            break;
          }
        }

        // ground_yより上にある数字たちを集める
        int count = 0;
        for (int y = ground_y - 1; y >= 0; y--) {
          int shift = 4 * (kWIDTH - 1 - x);
          PackedCells target_cell = (cells[y] & (0b1111ULL << shift));
          if (target_cell != 0ULL) {
            disappear_cells[y] |= target_cell;
            appear_cells[ground_y - count] |= target_cell;
            count++;
          }
        }
      }

      for (int y = 0; y < kDANGER_HEIGHT; y++) {
        // ブロックの移動 = ブロックの消去＋ブロックの出現
        cells[y] ^= disappear_cells[y];
        cells[y] |= appear_cells[y];
      }
    }

    // 消えるブロックを消す
    {
      PackedCells disappear_cells[kDANGER_HEIGHT] = { };
      for (int y = kDANGER_HEIGHT - 1; y > 0; y--) {
        if (cells[y] == 0ULL) {
          continue;
        }

        // 1行内で消える場合
        for (int x = 8; x >= 0; x--) {
          // 下位8bitを取り出す
          PackedCells mask = cells[y] & (0b11111111ULL << (4 * x));
          if (line_disappear_mask[mask >> (4 * x)]) {
            update = true;
            disappear_cells[y] |= mask;
          }
        }

        // 2行内で消える場合
        if (cells[y - 1] == 0ULL) {
          continue;
        }

        for (int x = 9; x >= 0; x--) {
          // 引いてくる表のインデックス
          PackedCells mask = cells[y] & (0xFULL << (4 * x));
          PackedCells mask2 = cells[y - 1] & ((x > 0)? (0xFFFULL << (4 * (x - 1))) : 0xFFULL);

          /**
           * デバッグ用
           * もう一度同じ条件で関数を呼び、デバッグをしやすくする。
           * もちろん無限ループになるので、gdbなどの使用を想定。
          if (something strange) {                         (e.g. if (((mask >> (4 * x)) >= 0b1111)) )
            std::cerr << "----- PANIC -----" << std::endl;
            start_position.Simulate(pack, action);
          }
          */

          // 消える対象のブロックは、この配列に前計算してある
          PackedCells result = other_disappear_mask[mask >> (4 * x)][(x > 0)? (mask2 >> (4 * (x - 1))) : (mask2 << 4)];

          if (result != 0ULL) {
            update = true;
            disappear_cells[y] |= mask;
            if (x == 0) {
              disappear_cells[y - 1] |= (result >> 4);
            } else {
              disappear_cells[y - 1] |= result << (4 * (x - 1));
            }
          }
        }
      }

      for (int y = kDANGER_HEIGHT - 1; y > 0; y--) {
        // 行ごとに消えるブロックたちがdisappear_cellsに集まっているので、
        // xorを取ることでブロックを消去する
        cells[y] ^= disappear_cells[y];
      }
    }

    if (!update) {
      // 消えたブロックがなければ、連鎖終了
      break;
    }

    chains++;
  }

  score.chain_score = chain_scores[chains];
  score.chain_count = chains;
  return score;
}


bool Position::IsGameOver() const {
  return (cells[2] > 0ULL);
}

void Position::Attacked(int attack_num) {
  if (attack_num == 0) {
    return;
  }

  for (int i = 0; i < attack_num; i++) {
    if (cells[i] != 0ULL) {
      break;
    }
    cells[i] = 806308527035ULL;  // 1段お邪魔
  }

  Simulate(Pack(), Action(NO_ACTION_TYPE));
}

bool Position::operator==(const Position& position) const {
  for (int y = 0; y < kDANGER_HEIGHT; y++) {
    if (position.cells[y] != cells[y]) {
      return false;
    }
  }

  return true;
}

bool Position::operator!=(const Position& position) const {
  return !(position == *this);
}
