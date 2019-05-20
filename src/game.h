#ifndef GAME_H_
#define GAME_H_

#include "types.h"
#include "position.h"
#include "pack.h"

/**
 * ゲームの情報を管理するクラス
 */
class Game {
public:
  int turn;  // 現在のターン数
  int remain_time[COLOR_NB];  // 残り思考時間
  int ojama_stock[COLOR_NB];  // お邪魔ストック
  int skills[COLOR_NB];  // スキルゲージ
  int scores[COLOR_NB];  // スコア

  Pack packs[kTURN_MAX];
  Position positions[COLOR_NB];  // 現在の局面

  // ゲーム開始時の入力を受け取る
  void GetInitInput();

  // ターン開始時の入力を受け取る
  void GetTurnInput();

  /**
   * デバッグ用
   * 標準エラー出力に局面を表示する
   */
  void Print(Color color) const;

  Game() = default;
};


#endif  // GAME_H_
