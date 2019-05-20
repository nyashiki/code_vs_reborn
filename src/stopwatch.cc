#include "stopwatch.h"

void Stopwatch::Start() {
  start = std::chrono::system_clock::now();
}

double Stopwatch::Elapsed() const {
  auto end = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}
