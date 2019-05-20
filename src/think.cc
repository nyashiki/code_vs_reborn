#include "think.h"
#include "stopwatch.h"
#include "score.h"
#include "eval.h"
#include "types.h"

#include <cstring>
#include <iostream>
#include <vector>
#include <queue>
#include <algorithm>
#include <random>
#include <thread>
#include <mutex>

namespace {

enum Mode {
  CHAIN_MODE, SKILL_MODE
};

std::mt19937_64 engine(20190328);

Game game;

/**
 * 与えられたpositionで、最大の連鎖スコアを全探索により探索する。
 */
Score CalculateCurrentChainScore(const Position& position, int ojama_stock, int depth, int depth_max, bool attacked_delay = false) {
  Score score_max;

  if (depth >= depth_max) {
    return Score();
  }

  for (int column = 0; column < 9; column++) {
    for (int rotation = 0; rotation < 4; rotation++) {
      Action action = Action(NORMAL, column, rotation);
      Position p = position;

      int os = ojama_stock;
      if (!attacked_delay && os >= kWIDTH) {
        p.Attacked();
        os -= kWIDTH;
      }

      Score score = p.Simulate(game.packs[game.turn + depth], action);

      if (p.IsGameOver()) {
        continue;
      }

      if (score.chain_count == 0 || (score.chain_count == 1 && game.packs[game.turn + depth].IsFlammable())) {
        score = CalculateCurrentChainScore(p, os, depth + 1, depth_max);
      }

      if (score.chain_count > score_max.chain_count) {
        score_max = score;
      }
    }
  }

  return score_max;
}

struct DepthFirstSearch {
  Position position;  // 探索開始局面
  int ojama_stock;  // 落下予定のお邪魔の数

  // 探索後、以下の変数たちに値が格納される
  Score score;
  Action action;

  // FillOut系の関数を呼ぶと以下の変数たちに値が格納される
  Score op_scores[8];  // n手後までに獲得できる相手の点数
  Score op_damaged_scores[8];  // お邪魔を受けた場合
  Score op_eval;
  Score op_skill_score;

  DepthFirstSearch(): score(Score()), action(Action(NORMAL, 0, 0)) { }

  void FillOutOpScoreTable(const Position& current_position, int depth, int depth_max, int ojama_stock, bool parallel = false) {
    if (depth >= depth_max) {
      for (int i = depth; i < 8; i++) {
        op_scores[i] = op_scores[i - 1];
      }

      return;
    }

    if (depth == 0) {
      memset(op_scores, 0, sizeof(op_scores));
      op_eval = Eval::EraseOne(game.positions[BLACK]);
    } else {
      if (op_scores[depth - 1].chain_count > op_scores[depth].chain_count) {
        op_scores[depth] = op_scores[depth - 1];
      }
    }

    Position position = current_position;
    if (ojama_stock >= kWIDTH) {
      position.Attacked();
      ojama_stock -= kWIDTH;
    }

    std::vector<std::thread> workers;
    std::mutex mtx;

    auto search_func = [this, &mtx, &position, &depth, &depth_max, &ojama_stock](int column, int rotation) {
      Action action = Action(NORMAL, column, rotation);
      Position next_position = position;
      Score score = next_position.Simulate(game.packs[game.turn + depth], action);

      if (next_position.IsGameOver()) {
        return;
      }

      if (score.chain_count == 0 || (score.chain_count == 1 && game.packs[game.turn + depth].IsFlammable())) {
        FillOutOpScoreTable(next_position, depth + 1, depth_max, ojama_stock);
      } else {
        std::lock_guard<std::mutex> lk(mtx);
        if (score.chain_count > op_scores[depth].chain_count) {
          op_scores[depth] = score;
        }
      }
    };

    for (int column = 0; column < 9; column++) {
      for (int rotation = 0; rotation < 4; rotation++) {
        if (parallel) {
          workers.emplace_back(search_func, column, rotation);
        } else {
          search_func(column, rotation);
        }
      }
    }

    if (parallel) {
      for (auto& worker : workers) {
        worker.join();
      }
    }
  }

  void FillOutOpDamagedScoreTable(const Position& current_position, int depth, int depth_max, bool parallel = false) {
    if (depth >= depth_max) {
      for (int i = depth; i < 8; i++) {
        op_damaged_scores[i] = op_damaged_scores[i - 1];
      }

      return;
    }

    if (depth == 0) {
      memset(op_damaged_scores, 0, sizeof(op_damaged_scores));
    } else {
      if (op_damaged_scores[depth - 1].chain_count > op_damaged_scores[depth].chain_count) {
        op_damaged_scores[depth].chain_count = op_damaged_scores[depth - 1].chain_count;
      }
    }

    Position position = current_position;
    if (game.ojama_stock[BLACK] >= kWIDTH || depth > 0) {
      position.Attacked();
    }

    std::vector<std::thread> workers;
    std::mutex mtx;

    auto search_func = [this, &mtx, &position, &depth, &depth_max](int column, int rotation) {
      Action action = Action(NORMAL, column, rotation);
      Position next_position = position;
      Score score = next_position.Simulate(game.packs[game.turn + depth], action);

      if (next_position.IsGameOver()) {
        return;
      }

      if (score.chain_count == 0 || (score.chain_count == 1 && game.packs[game.turn + depth].IsFlammable())) {
        FillOutOpDamagedScoreTable(next_position, depth + 1, depth_max);
      } else {
        std::lock_guard<std::mutex> lk(mtx);
        if (score.chain_count > op_damaged_scores[depth].chain_count) {
          op_damaged_scores[depth] = score;
        }
      }
    };

    for (int column = 0; column < 9; column++) {
      for (int rotation = 0; rotation < 4; rotation++) {
        if (parallel) {
          workers.emplace_back(search_func, column, rotation);
        } else {
          search_func(column, rotation);
        }
      }
    }

    if (parallel) {
      for (auto& worker : workers) {
        worker.join();
      }
    }
  }

  /**
   * 深さ優先探索により、positionのscoreを算出する。
   * 大きな連鎖をするように探索する。
   */
  Score ChainSearch(int depth, int depth_max, bool parallel = false) {
    if (depth >= depth_max) {
      // 最大探索深さに到達

      /**
       * 2019/03/30
       *  評価関数としてどのようなものを用いれば良いか検討中。
       *  EraseOne()では、空き升に隣接するブロックを1つ消し、連鎖の得点を見るものであるが、
       *  本当にそのような消し方ができるかわからないというデメリットがある。
       */
      Point erase_point;
      int erase_number = -1;
      position.Attacked(ojama_stock / kWIDTH);

      if (position.IsGameOver()) {
        return Score(0, 0, -INF + depth, 0);
      }

      Score score;
      if (game.ojama_stock[WHITE] < kWIDTH && position.GetPackedCells(6) == 0) {
        position.Attacked(4);
        score = Eval::EraseOne(position, false, &erase_point, &erase_number);
      } else {
        score = Eval::EraseOne(position, false, &erase_point, &erase_number);
        score.heuristic_score -= 20;
      }

      // そのblockを消すために、実際に必要な手数は少ないほうが良い
      if (erase_number != -1) {
        int require_turn = 0;
        for (int i = game.turn + depth; i < 500; i++) {
          if (game.packs[i].Count(10 - erase_number) > 0) {
            require_turn = i - (game.turn + depth);
            break;
          }
        }
        score.heuristic_score -= 3 * require_turn;
      }

      // ブロックの数を評価
      for (int y = 5; y < kDANGER_HEIGHT; y++) {
        for (int x = 0; x < kWIDTH; x++) {
          int num = position.Get(y, x);
          if (num != 0 && num != 11) {
            score.heuristic_score++;
          }
        }
      }

      return score;
    }

    Score best_score = Score(0, 0, -INF + depth, 0);
    Action best_action = Action(NORMAL, 0, 0);
    std::mutex mtx;

    Position current_position = position;
    int current_ojama_stock = ojama_stock;

    if (current_ojama_stock >= kWIDTH || (depth > 0 && (op_scores[0].chain_count >= 11))) {
      // お邪魔ブロックが降ってくる または
      // 相手が1ターン前に攻撃している可能性がある場合には、お邪魔ブロックが降ってくる前提で考える
      current_position.Attacked();
      current_ojama_stock -= kWIDTH;
    }

    Score my_skill_score, my_eval, op_damaged_score, damaged_score;

    if (depth == 0) {
      my_eval = Eval::EraseOne(game.positions[WHITE]);

      {
        Position my_position = game.positions[WHITE];
        my_skill_score = my_position.Simulate(Pack(), Action(SKILL));
      }

      {
        Position op_position = game.positions[BLACK];
        op_skill_score = op_position.Simulate(Pack(), Action(SKILL));
      }

      int ojama_predict = std::max(op_scores[0].chain_score / 2, op_skill_score.explosion_score / 2);
      damaged_score = CalculateCurrentChainScore(current_position, ojama_predict, 1, 4);

      if (game.skills[WHITE] >= 80) {
        if (my_skill_score.explosion_score >= 10 * kWIDTH && game.ojama_stock[BLACK] < kWIDTH && my_skill_score.explosion_score + 2 * game.ojama_stock[BLACK] >= 2 * kWIDTH && my_skill_score.explosion_score >= op_scores[0].GetScoreSum()) {
          if (game.skills[BLACK] < 80 || my_skill_score.explosion_score >= op_skill_score.GetScoreSum()) {
            this->score = my_skill_score;
            this->action = Action(SKILL);

            return my_skill_score;
          }
        }
      }
    }

    std::vector<std::thread> workers;

    for (int column = 0; column < 9; column++) {
      for (int rotation = 0; rotation < 4; rotation++) {
        auto search_func = [this, &mtx, &current_position, &current_ojama_stock, &depth, &depth_max, &my_skill_score, &my_eval, &op_damaged_score, &damaged_score, &best_score, &best_action](int column, int rotation) {
          DepthFirstSearch dfs;
          dfs.position = current_position;
          dfs.ojama_stock = current_ojama_stock;
          memcpy(dfs.op_scores, this->op_scores, sizeof(this->op_scores));  // ToDo: コピーが必要のない実装
          memcpy(dfs.op_damaged_scores, this->op_damaged_scores, sizeof(this->op_damaged_scores));  // ToDo: コピーが必要のない実装
          dfs.op_eval = this->op_eval;
          dfs.op_skill_score = this->op_skill_score;

          Score current_score = dfs.position.Simulate(game.packs[game.turn + depth], Action(NORMAL, column, rotation));

          if (dfs.position.IsGameOver()) {
            return;
          }

          Score future_score = Score();
          /**
           * 連鎖をしていない、または回避できない1連鎖の場合には、1手進めそこでのスコアを用いる。
           * 連鎖をした場合には、探索を打ち切り、現局面でのスコアを用いる。
           */
          if (current_score.chain_count == 0 || (current_score.chain_count == 1 && game.packs[game.turn + depth].IsFlammable())) {
            future_score = dfs.ChainSearch(depth + 1, depth_max);
          } else {
            future_score = dfs.ChainSearch(depth_max, depth_max);
          }

          Score score = Score(current_score.chain_score + future_score.chain_score,
                              0,
                              current_score.heuristic_score + future_score.heuristic_score,
                              current_score.chain_count);
          score.chain_count = current_score.chain_count;

          // 相手よりも大きな連鎖がある場合には、連鎖をして邪魔をする。
          if (((game.scores[WHITE] < 10 && current_score.chain_count >= 12) || (game.scores[WHITE] >= 10 && current_score.chain_count >= 11)) && ((game.skills[BLACK] + 8 * depth >= 80) || (game.skills[BLACK] + 8 * depth < 80 && current_score.chain_score - 2 * game.ojama_stock[WHITE] >= op_scores[depth].chain_score && current_score.chain_score - 2 * game.ojama_stock[WHITE] >= op_damaged_scores[depth + 2].chain_score)) && current_score.chain_score > (game.scores[BLACK] - game.scores[WHITE])) {
             score.heuristic_score += 10000000 - 1000 * depth - 110 * current_ojama_stock;
          }

          if (current_score.chain_count > 5) {
            score.heuristic_score -= 10;
          }

          if (depth == 0) {
            for (int y = 3; y < kDANGER_HEIGHT; y++) {
              for (int x = 0; x < kWIDTH; x++) {
                if (dfs.position.Get(y, x) == 5) {
                  score.heuristic_score++;
                }
              }
            }

            // 相手がデンジャーライン直下まで積みあがっている場合
            if (op_scores[0].chain_count == 0 && current_score.chain_score / 2 + game.ojama_stock[BLACK] >= kWIDTH && current_score.chain_score > op_scores[1].GetScoreSum() && game.ojama_stock[BLACK] < kWIDTH && game.positions[BLACK].GetPackedCells(3) != 0ULL) {
              score.heuristic_score += 700000;
            }
          }

          if (dfs.position.GetPackedCells(3) != 0ULL) {
            score.heuristic_score -= 2000;
          }

          for (int danger = 4; danger < 8; danger++) {
            if (dfs.position.GetPackedCells(danger) != 0ULL) {
              score.heuristic_score -= 8 - danger;
            }
          }

          {
            std::lock_guard<std::mutex> lk(mtx);

            if (score.GetScoreSum() > best_score.GetScoreSum()) {
              // より良い行動を発見

              best_score = score;
              best_action = Action(NORMAL, column, rotation);
            }
          }
        };

        if (parallel) {
          workers.emplace_back(search_func, column, rotation);
        } else {
          search_func(column, rotation);
        }
      }
    }

    if (parallel) {
      // 並列化している場合は、全スレッドが探索を終了するのを待つ
      for (auto& worker : workers) {
        worker.join();
      }
    }

    // スキルを使用する以外は負けの場合
    if (depth == 0 && best_score.GetScoreSum() == -INF && game.skills[WHITE] >= 80) {
      this->score = my_skill_score;
      this->action = Action(SKILL);

      return my_skill_score;
    }

    this->score = best_score;
    this->action = best_action;
    return best_score;
  }

  /**
   * 深さ優先探索により、positionのscoreを算出する。
   * 大きな爆発を起こすように探索する。
   */
  Score SkillSearch(int skill_point, int depth, int depth_max, bool parallel = false) {
    if (depth >= depth_max) {
      if (skill_point >= 80) {
        Score score = position.Simulate(Pack(), Action(SKILL));
        score.heuristic_score -= ojama_stock;
        score.heuristic_score += 10000;

        return score;
      } else {
        Score score;

        // 5の数を数える
        bool five_and_adjacents[kDANGER_HEIGHT][kWIDTH] = { };
        int heights[kWIDTH] = { };

        for (int y = 0; y < kDANGER_HEIGHT; y++) {
          for (int x = 0; x < kWIDTH; x++) {
            if (position.Get(y, x) != 0) {
              heights[x] = y;
            }

            if (position.Get(y, x) == 5) {
              five_and_adjacents[y][x] = true;

              const int dy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
              const int dx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
              for (int i = 0; i < 8; i++) {
                int ny = y + dy[i];
                int nx = x + dx[i];
                if (ny < 0 || ny >= kDANGER_HEIGHT || nx < 0 || nx >= kWIDTH) {
                  continue;
                }
                five_and_adjacents[ny][nx] = true;
              }
            }
          }
        }
        int explosion_count = 0;
        for (int y = 0; y < kDANGER_HEIGHT; y++) {
          for (int x = 0; x < kWIDTH; x++) {
            if (five_and_adjacents[y][x]) {
              explosion_count++;
            }
          }
        }

        // 高さの分散を計算する
        double var = 0;
        double mu = 0;
        for (int x = 0; x < kWIDTH; x++) {
          mu += heights[x];
        }
        mu /= kWIDTH;
        for (int x = 0; x < kWIDTH; x++) {
          var += (heights[x] - mu) * (heights[x] - mu);
        }
        var /= kWIDTH;

        score.heuristic_score += 5 * explosion_count - var;
        score.heuristic_score += skill_point;

        return score;
      }
    }

    Score op_damaged_score, op_skill_score;
    if (depth == 0) {
      {
        Position op_position = game.positions[BLACK];
        if (game.skills[BLACK] >= 80) {
          op_skill_score = op_position.Simulate(Pack(), Action(SKILL));
        }
      }

      op_damaged_score = CalculateCurrentChainScore(game.positions[BLACK], 5 * kWIDTH, 1, 4);
    }

    Position current_position = position;
    int current_ojama_stock = ojama_stock;
    if (current_ojama_stock >= kWIDTH) {
      current_position.Attacked();
      current_ojama_stock -= kWIDTH;
    }

    Score best_score = Score(0, 0, -INF + depth, 0);
    Action best_action = Action(NORMAL, 0, 0);

    if (skill_point >= 80) {
      Position skill_position = position;
      best_score = skill_position.Simulate(Pack(), Action(SKILL));
      best_action = Action(SKILL);

      if (position.GetPackedCells(9) != 0ULL && best_score.explosion_score - 2 * game.ojama_stock[WHITE] >= 10 * kWIDTH) {
        best_score.heuristic_score += 100000 - 100 * depth - 11 * ojama_stock;
      }
      if (best_score.explosion_score < 100) {
        best_score.heuristic_score -= 100;
      }

      if (depth == 0) {
        // 今SKILLを打たないと、打てなくなってしまう
        if (skill_point < 92 && op_scores[0].chain_count >= 3 && position.GetPackedCells(6) != 0ULL) {
          best_score.heuristic_score += 300000;
        }
      }
      best_score.heuristic_score += skill_point * 1.5;
    }

    std::mutex mtx;

    auto search_func = [&mtx, &skill_point, &current_position, &current_ojama_stock, &depth, &depth_max, &best_score, &best_action](int column, int rotate){
      DepthFirstSearch dfs;
      dfs.position = current_position;
      dfs.ojama_stock = current_ojama_stock;

      Score current_score = dfs.position.Simulate(game.packs[game.turn + depth], Action(NORMAL, column, rotate));

      if (dfs.position.IsGameOver()) {
        return;
      }

      int next_skill_point = (current_score.chain_count == 0)? skill_point : skill_point + 8;
      Score score = dfs.SkillSearch(next_skill_point, depth + 1, depth_max);
      score.chain_score = 0;  // 連鎖による得点は評価しない

      for (int danger = 4; danger < kDANGER_HEIGHT; danger++) {
        if (dfs.position.GetPackedCells(danger) != 0ULL) {
          score.heuristic_score -= kDANGER_HEIGHT - danger;
        }
      }

      if (skill_point <= 92) {
        if (current_score.chain_count == 1) {
          score.heuristic_score += 10;
        } else if (current_score.chain_count == 3) {
          score.heuristic_score += 2;
        } else {
          score.heuristic_score += 1;
        }
      }

      {
        std::lock_guard<std::mutex> lk(mtx);

        if (score.GetScoreSum() > best_score.GetScoreSum()) {
          best_score = score;
          best_score.chain_count = current_score.chain_count;
          best_action = Action(NORMAL, column, rotate);
        }
      }
    };

    std::vector<std::thread> workers;

    for (int column = 0; column < 9; column++) {
      for (int rotate = 0; rotate < 4; rotate++) {
        if (parallel) {
          workers.emplace_back(search_func, column, rotate);
        } else {
          search_func(column, rotate);
        }
      }
    }

    if (parallel) {
      for (auto& worker : workers) {
        worker.join();
      }
    }

    this->score = best_score;
    this->action = best_action;
    return best_score;
  }
};

/**
 * ビームサーチによる探索。
 */
struct BeamSearch {
  static inline const int kSEARCH_DEPTH = 21;

  Position position;
  Score score;
  Action action_sequence[kSEARCH_DEPTH + 2];
  int require_turn;
  std::vector<BeamSearch> states, next_states;

  bool operator>(const BeamSearch& beam_search) const {
    return score.GetScoreSum() > beam_search.score.GetScoreSum();
  }

  BeamSearch(): score(Score()), require_turn(INF) {

  }

  void Init() {
    states.reserve(100000 * 36);
    next_states.reserve(100000 * 36);
  }

  void Start(int target_chain_count, int search_width = 5000, bool use_sides = true) {
    std::mutex mtx;

    BeamSearch flammable_best;


    Stopwatch sw;
    sw.Start();

    for (int i = 0; i < kSEARCH_DEPTH + 2; i++) {
      action_sequence[i] = Action();
    }

    {
      states.clear();
      flammable_best = BeamSearch();

      // rootを登録
      BeamSearch beam_search;
      beam_search.position = position;
      states.push_back(beam_search);
    }

    for (int turn = 0; turn < kSEARCH_DEPTH; turn++) {
      if (turn > kSEARCH_DEPTH - 4) {
        search_width = 5000;
      }

      next_states.clear();

      // 目標連鎖数を最短で見つけたいため、
      // 目標連鎖数を達成している場合には、それ以上深く探索する必要がない
      if (flammable_best.score.chain_count >= target_chain_count) {
        break;
      }

      if (states.size() == 0) {
        break;
      }

      // search_widthよりも保持している状態の個数が少ないときに、バグが発生しないように注意する
      int sort_size = std::min((int)states.size(), search_width);
      std::partial_sort(states.begin(), states.begin() + sort_size, states.end(), std::greater<BeamSearch>());

      int counter = 0;

      auto search_func = [this, &mtx, &sw, &target_chain_count, &counter, &search_width, &use_sides, turn, &flammable_best]() {
        while(true) {
          BeamSearch beam_search;
          {
            std::lock_guard<std::mutex> lk(mtx);
            if (counter == (int)states.size()) {
              break;
            }

            if (counter == search_width) {
              break;
            }

            beam_search = states[counter];
            counter++;
          }

          for (int column = 0; column < 9; column++) {
            if (!use_sides) {
              if (column == 0) {
                continue;
              }
            }

            // 最初の探索では、真ん中に置くことしか考えない
            if (game.turn == 0 && turn == 0 && column != 4) {
              continue;
            }

            for (int rotate = 0; rotate < 4; rotate++) {
              // 思考時間が18秒超えたら打ち切り
              if (sw.Elapsed() > 18000) {
                return;
              }

              Action action = Action(NORMAL, column, rotate);
              BeamSearch next = beam_search;

              Score current_score = next.position.Simulate(game.packs[game.turn + turn], action);

              if (next.position.GetPackedCells(3) != 0ULL) {
                continue;
              }

              next.action_sequence[turn] = action;

              if (current_score.chain_count > 1) {
                next.score = current_score;
                next.require_turn = turn;

                // 目標連鎖数に達した場合は、連鎖以外のものを評価
                if (next.score.chain_count >= target_chain_count) {
                  // ブロックの数を評価
                  for (int y = 5; y < kDANGER_HEIGHT; y++) {
                    for (int x = 0; x < kWIDTH; x++) {
                      int num = next.position.Get(y, x);
                      if (num != 0 && num != 11) {
                        next.score.heuristic_score++;
                      }
                    }
                  }
                }

                {
                  //
                  // 無理に連鎖を大きくしにいかない。
                  // 狙った連鎖量をできるだけ早く撃つことを目指す。
                  //
                  std::lock_guard<std::mutex> lk(mtx);
                  if (next.score.chain_count >= target_chain_count && flammable_best.score.chain_count >= target_chain_count) {
                    if (turn < flammable_best.require_turn) {
                      flammable_best = next;
                    } else if (turn == flammable_best.require_turn) {
                      if (next.score.chain_count > flammable_best.score.chain_count) {
                        flammable_best = next;
                      } else if (next.score.chain_count == flammable_best.score.chain_count) {
                        if (next.score.heuristic_score > flammable_best.score.heuristic_score) {
                          flammable_best = next;
                        }
                      }
                    }
                  } else {
                    if (next.score.chain_count > flammable_best.score.chain_count) {
                      flammable_best = next;
                    } else if (next.score.chain_count == flammable_best.score.chain_count) {
                      if (next.require_turn < flammable_best.require_turn) {
                        flammable_best = next;
                      }
                    }
                  }
                }
              } else {
                Point erase_point;
                if (game.turn == 0) {
                  if (next.position.GetPackedCells(6) == 0ULL) {
                    Position damaged_position = next.position;
                    damaged_position.Attacked(4);
                    next.score = Eval::EraseOne(damaged_position, false, &erase_point, nullptr);
                  }
                } else {
                  next.score = Eval::EraseOne(next.position, false, &erase_point, nullptr);
                }

                // なるべく1連鎖をしない
                if (current_score.chain_count == 1) {
                  next.score.heuristic_score -= 1;
                }

                // 発火点が最下段はかなり悪い
                if (erase_point.y == kDANGER_HEIGHT - 1) {
                  next.score.heuristic_score -= 50;
                }

                if (game.turn > 0) {
                  // 2回目以降の連鎖構築では、相手が途中で攻撃してくる可能性が高いため、
                  // 発火点を高い場所に構える
                  if (erase_point.y == kDANGER_HEIGHT - 2) {
                    next.score.heuristic_score -= 40;
                  } else if (erase_point.y == kDANGER_HEIGHT - 3) {
                    next.score.heuristic_score -= 10;
                  } else if (erase_point.y == kDANGER_HEIGHT - 4) {
                    next.score.heuristic_score -= 5;
                  }
                  next.score.heuristic_score -= 2 * erase_point.y;
                }

                // 最上段は回避する
                if (next.position.GetPackedCells(3) != 0ULL) {
                  next.score.heuristic_score -= 1000;
                }

                for (int danger = 4; danger < 10; danger++) {
                  // あまり高く積まない方が良い
                  if (next.position.GetPackedCells(danger) != 0ULL) {
                    next.score.heuristic_score -= 5;
                  }
                }

                {
                  std::lock_guard<std::mutex> lk(mtx);
                  next_states.push_back(next);
                }
              }
            }
          }
        }
      };

#ifdef SERVER
      // 1スレッドで探索
      search_func();
#else
      // 複数スレッドで探索
      const int kWORKER_NUM = 16;
      std::vector<std::thread> workers;
      for (int worker_count = 0; worker_count < kWORKER_NUM; worker_count++) {
        workers.emplace_back(search_func);
      }
      for (auto& worker : workers) {
        worker.join();
      }
#endif

      states.swap(next_states);
    }

    score = flammable_best.score;
    require_turn = flammable_best.require_turn;
    for (int i = 0; flammable_best.action_sequence[i].action_type != NO_ACTION_TYPE; i++) {
      action_sequence[i] = flammable_best.action_sequence[i];
    }
  }
};

BeamSearch beam_search;

}  // namespace

void Think::Init() {
  beam_search.Init();
}

Action Think::Start(const Game& g) {
  Stopwatch sw;
  sw.Start();

  game = g;
  static bool beam_search_flag = true;
  static std::queue<Action> action_queue;
  static Mode mode = CHAIN_MODE;

  /**
   * 連鎖モード &&
   * ビームサーチフラグ オン &&
   * 残り時間が40秒以上 &&
   * お邪魔が降ってこない &&
   * ブロックが上から5段目以上にない &&
   *
   * その場合には、ビームサーチで目標連鎖数だけの連鎖ができるか探索。
   */
  if (mode == CHAIN_MODE && beam_search_flag && game.remain_time[WHITE] > 40 * 1000 && game.ojama_stock[WHITE] < kWIDTH && game.positions[WHITE].GetPackedCells(5) == 0ULL) {
    std::cerr << "----- BEAM SEARCH -----" << std::endl;
    beam_search.position = game.positions[WHITE];
    int target_chain_count = (game.turn == 0)? 99 : 12;  // 0 ターン目はできるだけ大きい連鎖、1ターン目以降は12連鎖を目指す

    // ビーム幅
#ifdef SERVER
    int search_width = (game.turn == 0)? 6000 : 6000;
#else
    int search_width = (game.turn == 0)? 30000 : 50000;
#endif

    bool use_sides = (game.turn > 0);  // 0ターン目は一番端の列を使わない

    beam_search.Start(target_chain_count, search_width, use_sides);  // 探索開始

    std::cerr << "expected chain: " << beam_search.score.chain_count << " in " << beam_search.require_turn << " turn [" << (int)sw.Elapsed() << " ms]" << std::endl;
    std::cerr << "-----------------------" << std::endl;

    // 過去の探索結果が格納されている場合は、消去しておく
    while (!action_queue.empty()) {
      action_queue.pop();
    }

    // 11連鎖を作れなかった場合や、連鎖数 < ターン数の場合には、SKILL型へ移行
    if (beam_search.score.chain_count < 11 && beam_search.score.chain_count < beam_search.require_turn) {
      mode = SKILL_MODE;
    } else {
      /**
       *  何手先まで探索結果を保存するか
       *   - 0ターン目は発火直前まで保存
       *   - それ以降は、発火直前 - 3 ターンまで保存
       *       探索結果が保持されていない場合は、深さ4の全探索を行うため、
       *       この発火が全探索で発見される
       * 連鎖を大きくすることよりも、途中で相手を攻撃する方が有力な場合があるため、
       * 最後まで登録しない。
       */
#ifdef SERVER
      int cache_depth = (game.turn == 0)? beam_search.require_turn - 2 : std::min(7, beam_search.require_turn - 2);
#else
      int cache_depth = (game.turn == 0)? beam_search.require_turn - 3 : std::min(7, beam_search.require_turn - 3);
#endif
      for (int i = 0; i < cache_depth && i < beam_search.require_turn; i++) {
        action_queue.push(beam_search.action_sequence[i]);
      }
    }

    beam_search_flag = false;
  }

  if (mode == CHAIN_MODE && !action_queue.empty() && game.ojama_stock[WHITE] < kWIDTH) {
    // 探索済みのものを使う
    std::cerr << "cache (" << beam_search.score.chain_count << " chain) [" << (int)sw.Elapsed() << " ms]" << std::endl;

    Action action = action_queue.front();
    action_queue.pop();
    return action;
  } else {
    // お邪魔が送られるなどして、探索結果が使えなくなった場合は、消去しておく
    beam_search_flag = false;
    beam_search.score = Score();
    while (!action_queue.empty()) {
      action_queue.pop();
    }
  }

  DepthFirstSearch dfs;

  // 相手の連鎖について計算しておく
#ifdef SERVER
  dfs.FillOutOpScoreTable(g.positions[BLACK], 0, 3, g.ojama_stock[BLACK]);
  dfs.FillOutOpDamagedScoreTable(g.positions[BLACK], 0, 3);
#else
  dfs.FillOutOpScoreTable(g.positions[BLACK], 0, 4, g.ojama_stock[BLACK], true);
  dfs.FillOutOpDamagedScoreTable(g.positions[BLACK], 0, 4, true);
#endif

THINK:
  dfs.position = g.positions[WHITE];
  dfs.ojama_stock = g.ojama_stock[WHITE];

#ifdef SERVER
  // サーバ用の設定
  if (mode == CHAIN_MODE) {
    dfs.ChainSearch(0, 3);
  } else {
    dfs.SkillSearch(game.skills[WHITE], 0, 3);
  }
#else
  int depth = (game.remain_time[WHITE] > 20 * 1000)? 4 : 3;  // 時間が20秒以上ある場合は深さ4、なければ深さ3の探索を行う
  if (mode == CHAIN_MODE) {
    dfs.ChainSearch(0, depth, true);
    if (dfs.score.GetScoreSum() < 20 && game.ojama_stock[WHITE] >= 3 * kWIDTH) {
      // 有力な連鎖が見つからない場合
      // お邪魔が降ってくる場合は、スキル型へ移行
      mode = SKILL_MODE;

      goto THINK;  // スキル型として再探索
    } else if (dfs.score.GetScoreSum() < 70 && game.ojama_stock[WHITE] < 2 * kWIDTH) {
      // 次のターンでビームサーチを行う
      beam_search_flag = true;
    }
  } else {
    dfs.SkillSearch(game.skills[WHITE], 0, depth, true);
  }
#endif

  if (dfs.score.chain_count > 6 || dfs.score.explosion_score > 0) {
    // 連鎖をした後、スキルを使用した後はビームサーチフラグをオンにする
    beam_search_flag = true;
  }

  // 標準エラー出力に色々表示
  std::cerr << "turn: " << game.turn << std::endl;
  if (mode == CHAIN_MODE) {
    std::cerr << "  mode: CHAIN" << std::endl;
  } else {
    std::cerr << "  mode: SKILL" << std::endl;
  }
  std::cerr << "  score:" << dfs.score.GetScoreSum() << std::endl;
  std::cerr << "  chain: " << dfs.score.chain_count << std::endl;
  std::cerr << "  explosion_score: " << dfs.score.explosion_score << std::endl;
  std::cerr << "  elapsed: " << (int)sw.Elapsed() << " ms" << std::endl;

  if (dfs.action.action_type == SKILL) {
    // スキル使用後は連鎖を探索
    mode = CHAIN_MODE;
  }

  return dfs.action;
}
