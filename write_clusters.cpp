#include <atomic>
#include <chrono>
#include <ctime>
#include <glob.h>
#include <iostream>
#include <fstream>
#include <queue>
#include <regex>
#include <sw/redis++/redis++.h>
#include <thread>
#include "constants.hpp"
#include "parallel_hashmap/phmap_dump.h"
#include <cassert>

using namespace sw::redis;

using namespace std::chrono;

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
  {'c', 2},
  {'d', 1},
  {'h', 0},
  {'s', 3}
};

#define MAX_SUITS 4

HashT<uint64_t, uint16_t> buckets;

std::atomic<uint64_t> num_keys;

void _process_file(std::string boards_filename) {
  auto redis = Redis("unix:///run/redis.sock/0");
  std::ifstream infile(boards_filename);
  std::string board;
  std::string space = " ";
  glob_t buf;
  char **cur;
  const std::regex base_regex(".*_([0-9]+).*");
  std::smatch base_match;
  std::regex_match(boards_filename, base_match, base_regex);
  std::queue<std::string> boards;
  while (std::getline(infile, board)) {
    boards.push(board);
  }

  auto pattern = "/home/asellerg/pkmeans/assignments/flop/" + base_match[1].str() + "/*";
  std::cout << pattern + "\n";
  glob(pattern.c_str(), 0, NULL, &buf);
  for (cur = buf.gl_pathv; *cur; cur++) {
    std::ifstream fp(*cur);
    std::string line;
    printf("reading %s...\n", *cur);
    auto pipeline = redis.pipeline();
    while (std::getline(fp, line)) {
      auto delimiter = line.find(space);
      std::string bucket_str = line.substr(delimiter);
      uint64_t board[7] = {0};
      auto key = boards.front().c_str();
      uint16_t bucket = std::atoi(bucket_str.c_str());
      for (int i = 0; i < strlen(key); i+=2) {
        int rank = ranks[key[i]];
        int suit = suits[key[i+1]];
        int card = ((13*suit)+rank);
        board[i/2] = card + 1;
      }
      uint64_t idx = (board[0]) | (board[1] << 8) | (board[2] << 16) | (board[3] << 24) | (board[4] << 32) | (board[5] << 40) | (board[6] << 48);
      buckets[idx] = bucket;
      assert(bucket <= 1024);
      pipeline.set(key, std::to_string(bucket));
      num_keys++;
      boards.pop();
      if (num_keys.load() % 100000 == 0) {
        printf("%ld keys loaded.\n", num_keys.load());
      }
    }
    pipeline.exec();
  }
}


int main( const int argc, const char *argv[] )
{
  num_keys.store(0);
  phmap::BinaryInputArchive ar_in("/home/asellerg/data/preflop_buckets.bin");
  printf("Loading phmap.\n");
  buckets.phmap_load(ar_in);
  printf("Loaded phmap: %ld.\n", buckets.size());
  std::vector<std::thread> threads(48);
  auto start = high_resolution_clock::now();
  glob_t buf;
  char **cur;
  std::string pattern = "/sda/data/phs_ec2_1024-512-512/flop_boards_*";
  glob(pattern.c_str(), 0, NULL, &buf);
  int i = 0;
  for (cur = buf.gl_pathv; *cur; cur++) {
    threads[i] = std::thread(_process_file, std::string(*cur));
    i++;
  }
  for (auto& thread : threads) {
     thread.join();
  }
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<seconds>(stop - start);
  printf("Total time: %d seconds.\n", duration.count());
  phmap::BinaryOutputArchive out("/home/asellerg/data/preflop_flop_buckets.bin");
  buckets.phmap_dump(out);
  return 0;
}
