#ifndef POSITION_H_
#define POSITION_H_

#include "types.h"
#include "action.h"
#include "pack.h"
#include "score.h"
#include <cinttypes>

typedef uint_fast64_t PackedCells;

/**
 * 1つの局面を管理するクラス
 */
class Position {
private:
  /**
   * ここで局面の横一列の数字列は、10^10までの数値で表すことができるので、64bit変数に収めることができる (10^10 < 2^64)。
   * そこで横一列は64bit変数で管理し、縦にそれを(18 + 1)個繋げたもので局面を管理する。
   * こうすることで単純に二次元配列で保存するより高速なコピーを可能にする。
   * また、このようにすることでデンジャーラインを超えたかどうかの判定を、(cells[2] > 0)と行うことができる (0が上であることに注意)。
   */
  PackedCells cells[kDANGER_HEIGHT];

public:
  Position();

  /**
   * 初期化
   * 本プログラム起動時に必ず呼ぶこと。
   */
  static void Init();

  /**
   * デバッグ用。
   * 標準エラー出力に現局面を表示する。
   */
  void Print() const;

  /**
   * 標準入力から局面の状態を受け取る
   */
  void GetInput();

  /**
   * cells[y][x]を取得する。
   */
  uint_fast64_t Get(int y, int x) const;

  /**
   * cells[y]を取得する。
   */
  PackedCells GetPackedCells(int y) const;

  /**
   * cells[y][x]にcellを設定する。
   */
  void Set(int y, int x, uint_fast64_t cell);

  /**
   * PackをActionで指定された状態で落とす。
   * お邪魔がある場合には、あらかじめpackに詰めておくこと。
   */
  Score Simulate(const Pack& pack, const Action& action);

  bool IsGameOver() const;

  /**
   * お邪魔ブロックの落下処理。
   */
  void Attacked(int attack_num = 1);

  bool operator==(const Position& position) const;
  bool operator!=(const Position& position) const;
};


#endif  // POSITION_H_
