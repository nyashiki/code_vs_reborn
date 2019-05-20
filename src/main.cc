#include "types.h"
#include "game.h"
#include "think.h"
#include "action.h"

#include <iostream>

int main() {
  Pack::Init();
  Position::Init();
  Think::Init();

  // はじめに、名前を出力
  std::cout << "nyashiki" << std::endl;

  Game game;
  game.GetInitInput();

  Position next_position;
  Action prev_action;

  bool is_first_input = true;

  while (true) {
    game.GetTurnInput();

    /**
     * デバッグ用
     * 与えられる局面が、
     * 前回の局面に前回の行動を行った結果と一致することを確認する。
     */
    /*
    if (!is_first_input) {
      if (next_position != game.positions[WHITE]) {
        std::cerr << "ERROR. PREDICTED POSITION IS NOT EQUAL." << std::endl;
        std::cerr << "game.turn: " << game.turn << std::endl;
        std::cerr << "----- predict -----" << std::endl;
        next_position.Print();

        std::cerr << "----- actual -----" << std::endl;
        game.Print(WHITE);
      }
    }
    */

    Action next_action = Think::Start(game);  // 自分の行動を探索
    std::cout << next_action.ToString() << std::endl;  // 標準出力に次の行動を出力

    /**
     * デバッグ用
     * なるはずの局面を用意しておく。
     */
    /*
    next_position = game.positions[WHITE];
    if (game.ojama_stock[WHITE] >= kWIDTH) {
      next_position.Attacked();
    }
    next_position.Simulate(game.packs[game.turn], next_action);

    is_first_input = false;
    prev_action = next_action;
    */
  }

  return 0;
}
