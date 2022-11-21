#include <chrono>
#include <cstdio>
#include <ctime>
#include <sw/redis++/redis++.h>
#include <iostream>
#include <thread>
#include "poker.h"
#include "pokerlib.h"
#include "constants.hpp"
#include "parallel_hashmap/phmap_dump.h"
using namespace std::chrono;

using namespace sw::redis;

hash_t buckets;
HashT<uint64_t, const char *> debug;

uint deck[52];

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

void _read_keys(std::string pattern) {
  auto cursor = 0LL;
  auto count = 500000;
  auto start = high_resolution_clock::now();
  auto redis = Redis("unix:///run/redis.sock/0");
  while (true) {
      std::unordered_set<std::string> keys;
      cursor = redis.scan(cursor, pattern, count, std::inserter(keys, keys.begin()));
      for (auto iter = keys.begin(); iter != keys.end(); ++iter) {
        const char *key = (*iter).c_str();
        uint64_t board[7] = {0};
        for (int i = 0; i < strlen(key); i+=2) {
          int rank = ranks[key[i]];
          int suit = suits[key[i+1]];
          int card = ((rank)*MAX_SUITS+(suit));
          board[i/2] = card + 1;
        }
        uint64_t idx = (board[0]) | (board[1] << 8) | (board[2] << 16) | (board[3] << 24) | (board[4] << 32) | (board[5] << 40) | (board[6] << 48);
        int bucket = stoi(*redis.get(key));
        if(buckets.count(idx)) {
          printf("%ld index for hand %s with bucket %d being overwritten with %d from hand %s.\n", idx, debug[idx], buckets[idx], bucket, key);
        }
        debug[idx] = key;
        buckets[idx] = bucket;
      }

      if (cursor == 0) {
        break;
      }
      auto stop = high_resolution_clock::now();
      auto duration = duration_cast<seconds>(stop - start);
      printf("%ld keys written @ %f keys/second.\n", buckets.size(), (1.0 * buckets.size()) / duration.count());
  }
}


int main( const int argc, const char *argv[] )
{
  init_deck(deck);
  std::vector<std::thread> threads(13);
  char ranks_as_str[14] = "23456789TJQKA";
  auto start = high_resolution_clock::now();
  for (int i = 0; i < 13; i++) {
    char pattern[3];
    pattern[0] = ranks_as_str[i];
    pattern[1] = '*';
    pattern[2] = '\0';
    threads[i] = std::thread(_read_keys, std::string(pattern));
  }

  for (auto& thread : threads) {
    thread.join();
  }
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<seconds>(stop - start);
  printf("Total time: %d seconds.\n", duration);
  phmap::BinaryOutputArchive out("./buckets.bin");
  buckets.phmap_dump(out);

  return 0;
}
