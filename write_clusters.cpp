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
#include <cassert>
#include <boost/program_options.hpp>

using namespace sw::redis;

using namespace std::chrono;

namespace po = boost::program_options;

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

std::atomic<uint64_t> num_keys;

void _process_file(std::string boards_filename, std::string base_dir, std::string street, int multiplier) {
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

  auto pattern = base_dir + "assignments/" + street + "/" + base_match[1].str() + "/*";
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
      bucket += (multiplier * 13);
      for (int i = 0; i < strlen(key); i+=2) {
        int rank = ranks[key[i]];
        int suit = suits[key[i+1]];
        int card = ((13*suit)+rank);
        board[i/2] = card + 1;
      }
      uint64_t idx = (board[0]) | (board[1] << 8) | (board[2] << 16) | (board[3] << 24) | (board[4] << 32) | (board[5] << 40) | (board[6] << 48);
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

  po::options_description desc(
      "write_clusters, reads from pkmeans text files to redis.\n");
  desc.add_options ()
    ("street",              po::value<std::string> ()->default_value ("flop"),                  "which street")
    ("base_dir",            po::value<std::string> ()->default_value (""),                      "base directory")
    ("multiplier",          po::value<int> ()->default_value (0),                               "multiplier");
  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv)
                .style(po::command_line_style::unix_style)
                .options(desc)
                .run(),
            vm);
  po::notify(vm);
  const auto street = vm["street"].as<std::string>();
  const auto base_dir = vm["base_dir"].as<std::string>();
  const auto multiplier = vm["multiplier"].as<int>();

  // check for help
  if (vm.count("help") || argc == 1) {
    std::cout << desc << std::endl;
    return 1;
  }

  num_keys.store(0);
  std::vector<std::thread> threads(128);
  auto start = high_resolution_clock::now();
  glob_t buf;
  char **cur;

  std::string pattern = base_dir + street + "_boards_*";
  std::cout << pattern << "\n";
  glob(pattern.c_str(), 0, NULL, &buf);
  int i = 0;
  for (cur = buf.gl_pathv; *cur; cur++) {
    threads[i] = std::thread(_process_file, std::string(*cur), base_dir, street, multiplier);
    i++;
  }
  for (auto& thread : threads) {
    if (thread.joinable()) {
     thread.join();
    }
  }
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<seconds>(stop - start);
  printf("Total time: %d seconds.\n", duration.count());
  return 0;
}
