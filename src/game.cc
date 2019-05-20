#include "game.h"

#include <cinttypes>
#include <cassert>
#include <iostream>
#include <string>

void Game::GetInitInput() {
  for (int t = 0; t < kTURN_MAX; t++) {
    packs[t].GetInput();
  }
}

void Game::GetTurnInput() {
  std::cin >> turn;

  for (int i = 0; i < 2; i++) {
    std::cin >> remain_time[i];
    std::cin >> ojama_stock[i];
    std::cin >> skills[i];
    std::cin >> scores[i];
    positions[i].GetInput();
  }
}

void Game::Print(Color color) const {
  positions[color].Print();
}
