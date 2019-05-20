#ifndef TYPES_H_
#define TYPES_H_

const int kTURN_MAX = 500;  // 最大ターン数
const int kDANGER_HEIGHT = 19;  // 落下処理を楽にするため、1つ多めに取っておく
const int kHEIGHT = 16;
const int kWIDTH = 10;

const int INF = 1e9;

/**
 * プレイヤーを表す列挙型
 * WHITEを左側のプレイヤ、BLACKを右側のプレイヤとする。
 */
enum Color {
  WHITE, BLACK,
  COLOR_NB
};

struct Point {
  int y, x;

  Point() = default;
  Point(int y, int x): y(y), x(x) { }
};

#endif  // TYPES_H_
