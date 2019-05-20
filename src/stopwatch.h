#ifndef STOPWATCH_H_
#define STOPWATCH_H_

#include <chrono>

class Stopwatch {
private:
  std::chrono::system_clock::time_point start;

public:
  void Start();

  double Elapsed() const;  // ミリ秒で返す
};

#endif  // STOPWATCH_H_
