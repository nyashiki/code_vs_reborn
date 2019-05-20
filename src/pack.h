#ifndef PACK_H_
#define PACK_H_

#include <cinttypes>
#include <string>

/**
 * 1つのPackを表す構造体
 */
struct Pack {
private:
  /**
   * 1つのpackは16bitで表すことができる。
   * 上位4bitごとに、左上、右上、「右下」、「左下」と格納することで、
   * 回転をbit shiftで表現することができる。
   */
  uint_fast16_t data_;

public:
  Pack() = default;
  Pack(uint_fast16_t data_);
  Pack(int upper_left, int upper_right, int lower_left, int lower_right);


  static void Init();

  void GetInput();
  void Set(int upper_left, int upper_right, int lower_left, int lower_right);

  Pack GetRotated(int rotate) const;
  uint_fast16_t GetTopLine() const;  // 上段のブロックたちを得る
  uint_fast16_t GetBottomLine() const; // 下段のブロックたちを得る

  bool IsFlammable() const;  // このPackを何もないところに落とすだけで、1連鎖が発生するかどうか

  int Count(int number) const;  // numberが何個含まれているかを返す

  bool operator==(const Pack& pack) const;
  std::string ToString() const;
};


#endif  // PACK_H_
