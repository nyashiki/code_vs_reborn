#ifndef SCORE_H_
#define SCORE_H_

struct Score {
  int chain_score;
  int explosion_score;
  int heuristic_score;

  int chain_count;

  Score(): chain_score(0), explosion_score(0), heuristic_score(0), chain_count(0) { }
  Score(int cs ,int es, int hs, int cc): chain_score(cs), explosion_score(es), heuristic_score(hs), chain_count(cc) { }

  int GetScoreSum() const {
    return chain_score + explosion_score + heuristic_score;
  }
};


#endif  // SCORE_H_
