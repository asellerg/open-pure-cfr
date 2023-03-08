#include <unordered_map>
#include "constants.hpp"
#include "parallel_hashmap/phmap_dump.h"
#include "lookup_buckets.hpp"

std::unordered_map<char, int> ranks = {
  {'2', 0},
  {'3', 1},
  {'4', 2},
  {'5', 3},
  {'6', 4},
  {'7', 5},
  {'8', 6},
  {'9', 7},
  {'T', 8},
  {'J', 9},
  {'Q', 10},
  {'K', 11},
  {'A', 12}
};

std::unordered_map<char, int> suits = {
  {'c', 3},
  {'d', 2},
  {'h', 1},
  {'s', 0}
};

#define MAX_SUITS 4

hash_t buckets;

void init() {
	phmap::BinaryInputArchive ar_in("/home/asellerg/data/buckets.bin");
	buckets.phmap_load(ar_in);
}

uint16_t get(char *hand) {
  uint64_t board[7] = {0};
  for (int i = 0; i < strlen(hand); i+=2) {
    int rank = ranks[hand[i]];
    int suit = suits[hand[i+1]];
    int card = ((rank)*MAX_SUITS+(suit));
    board[i/2] = card + 1;
  }
  uint64_t idx = (board[0]) | (board[1] << 8) | (board[2] << 16) | (board[3] << 24) | (board[4] << 32) | (board[5] << 40) | (board[6] << 48);
  return buckets[idx];
}