#include "pack.h"

#include <cstring>
#include <cassert>
#include <iostream>

namespace {

bool flammable_patterns[0xFFFF];  // 置くだけで1連鎖になってしまうpackたち
int number_count_table[10][0xFFFF];  // ある数字がこのpackに何個含まれているかを返す

}


void Pack::Init() {
  memset(flammable_patterns, 0, sizeof(flammable_patterns));
  memset(number_count_table, 0, sizeof(number_count_table));

  // それぞれの数字があるPackに何個含まれるかを，表に格納しておく
  for (int num1 = 0; num1 < 10; num1++) {
    for (int num2 = 0; num2 < 10; num2++) {
      for (int num3 = 0; num3 < 10; num3++) {
        for (int num4 = 0; num4 < 10; num4++) {
          int bit_pattern = (num1 << 12) | (num2 << 8) | (num3 << 4) | num4;
          if (num1 + num2 == 10 || num1 + num3 == 10 || num1 + num4 == 10 || num2 + num3 == 10 || num2 + num4 == 10 || num3 + num4 == 10) {
            flammable_patterns[bit_pattern] = true;
          }

          number_count_table[num1][bit_pattern]++;
          number_count_table[num2][bit_pattern]++;
          number_count_table[num3][bit_pattern]++;
          number_count_table[num4][bit_pattern]++;
        }
      }
    }
  }
}


Pack::Pack(uint_fast16_t data_): data_(data_) { }
Pack::Pack(int upper_left, int upper_right, int lower_left, int lower_right) {
  Set(upper_left, upper_right, lower_left, lower_right);
}

void Pack::GetInput() {
  uint_fast16_t d[4] = { };
  for (int i = 0; i < 4; i++) {
    std::cin >> d[i];
  }
  Set(d[0], d[1], d[2], d[3]);

  std::string end_str;
  std::cin >> end_str;
  assert(end_str == "END");
}

void Pack::Set(int upper_left, int upper_right, int lower_left, int lower_right) {
  data_ = (upper_left << 12) |
          (upper_right << 8) |
          (lower_right << 4) |
          lower_left;
}

Pack Pack::GetRotated(int rotate) const {
  assert(0 <= rotate && rotate < 4);

  if (rotate == 0) {
    return Pack(data_);
  } else if (rotate == 1) {
    return Pack((data_ >> 4) | ((data_ & 0b1111) << 12));
  } else if (rotate == 2) {
    return Pack((data_ >> 8) | ((data_ & 0b11111111) << 8));
  } else {
    return Pack(((data_ << 4) | (data_ >> 12)) & 0xFFFF);
  }
}

uint_fast16_t Pack::GetTopLine() const {
  return (data_ >> 8);
}

uint_fast16_t Pack::GetBottomLine() const {
  return (((data_ >> 4) & 0b1111) | ((data_ << 4) & 0b11110000));
}

bool Pack::IsFlammable() const {
  return flammable_patterns[data_];
}

int Pack::Count(int number) const {
  return number_count_table[number][data_];
}

bool Pack::operator==(const Pack& pack) const {
  return (this->data_ == pack.data_);
}

std::string Pack::ToString() const {
  std::string ret;

  ret += "[";
  ret += std::to_string((data_ & (0b1111 << 12)) >> 12) + ", ";
  ret += std::to_string((data_ & (0b1111 <<  8)) >>  8) + ", ";
  ret += std::to_string((data_ & (0b1111 <<  0)) >>  0) + ", ";
  ret += std::to_string((data_ & (0b1111 <<  4)) >>  4);
  ret += "]";

  return ret;
}
