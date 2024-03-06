/* pure_cfr_machine.cpp
 * Richard Gibson, Jul 1, 2013
 * Email: richard.g.gibson@gmail.com
 *
 * Contains the hand generation and Pure CFR tree walk methods.
 *
 * Copyright (C) 2013 by Richard Gibson
 */

/* C / C++ / STL includes */
#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <limits.h>
#include <thread>
#include "poker.h"
#include "constants.hpp"
#include "parallel_hashmap/phmap_dump.h"
#include "player_module.hpp"

/* C project_acpc_poker includes */
extern "C" {
}

/* Pure CFR includes */
#include "pure_cfr_machine.hpp"

hash_t cache;

std::unordered_map<int, std::string> action_abbrevs = {{0, "f"}, {1, "c"}, {2, "r0.33"}, {3, "r0.5"}, {4, "r0.66"}, {5, "r1"}, {6, "r1.3"}, {7, "rall"}};

std::unordered_map<int, std::string> round_abbrevs = {{0, "|p|"}, {1, "|f|"}, {2, "|t|"}, {3, "|r|"}};

using AvgStrategyMap = HashT<std::string, std::map<std::string, uint64_t>>;
AvgStrategyMap avg_strategy_dict;

std::unordered_map<std::string, uint64_t[ MAX_ABSTRACT_ACTIONS ]> preflop_strategy;

std::unordered_map<uint16_t, std::vector<uint16_t>> allowed_actions_cache;

std::unordered_map<uint16_t, std::string> allowed_actions_str_cache;

std::vector<std::string> ordered_actions = {"fold", "call", "raise 0.33", "raise 0.5", "raise 0.66", "raise 1", "raise 1.3", "raise all"};


PureCfrMachine::PureCfrMachine( const Parameters &params )
  : ag( params ),
    do_average( params.do_average )
{
  /* count up the number of entries required per round to store regret,
   * avg_strategy
   */
  size_t num_entries_per_bucket[ MAX_ROUNDS ];
  size_t total_num_entries[ MAX_ROUNDS ];
  memset( num_entries_per_bucket, 0,
	  MAX_ROUNDS * sizeof( num_entries_per_bucket[ 0 ] ) );
  memset( total_num_entries, 0, MAX_ROUNDS * sizeof( total_num_entries[ 0 ] ) );
  ag.count_entries( num_entries_per_bucket, total_num_entries );
  
  /* initialize regret and avg strategy */
  for( int r = 0; r < MAX_ROUNDS; ++r ) {
    if( r < ag.game->numRounds ) {
      /* Regret */
      switch( REGRET_TYPES[ r ] ) {
        case TYPE_INT:
        	regrets[ r ] = new Entries_der<int>( num_entries_per_bucket[ r ],
        					     total_num_entries[ r ] );
        	break;

        case TYPE_INT16_T:
          regrets[ r ] = new Entries_der<int16_t>( num_entries_per_bucket[ r ],
                       total_num_entries[ r ] );
          break;

        case TYPE_INT8_T:
          regrets[ r ] = new Entries_der<int8_t>( num_entries_per_bucket[ r ],
                       total_num_entries[ r ] );
          break;

        default:
        	fprintf( stderr, "unrecognized regret type [%d], "
        		 "note that type must be signed\n", REGRET_TYPES[ r ] );
        	exit( -1 );
      }
    } else {
      regrets[ r ] = NULL;
    }
  }

  for (int i = 0; i < ag.game->numPlayers; i++) {
    for( int r = 0; r < MAX_ROUNDS; ++r ) {
      if( r < ag.game->numRounds ) {
      	avg_strategy[ i ][ r ] = NULL;
      } else {
        /* Round out of range */
        avg_strategy[ i ][ r ] = NULL;
      }
    }
  }
}

PureCfrMachine::~PureCfrMachine( )
{
  for( int r = 0; r < MAX_ROUNDS; ++r ) {
    if( regrets[ r ] != NULL ) {
      delete regrets[ r ];
      regrets[ r ] = NULL;
    }
    for (int i = 0; i < ag.game->numPlayers; i++) {
      if( avg_strategy[ i ][ r ] != NULL ) {
        delete avg_strategy[ i ][ r ];
        avg_strategy[ i ][ r ] = NULL;
      }
    }
  }
}

void PureCfrMachine::load_phmap()
{

  std::ifstream infile("/home/asellerg/open-pure-cfr/preflop_strategy_gto_wizard.txt");
  std::string line;
  std::string space = " ";
  std::string comma = ",";
  while (std::getline(infile, line)) {
    auto delimiter = line.find(space);
    std::string info_set_str = line.substr(0, delimiter);
    std::vector<std::string> result;
    std::stringstream s_stream(line.substr(delimiter));
    while(s_stream.good()) {
       std::string substr;
       getline(s_stream, substr, ','); //get first string delimited by comma
       result.push_back(substr);
    }
    int j = 0;
    for (int i = 0; i < MAX_ABSTRACT_ACTIONS; i++) {
      if (i == 0 || i == 1 || i == 5 || i == 7) {
        preflop_strategy[info_set_str][i] = std::atoi(result[j].c_str());
        j++;
      }
    }
  }
  phmap::BinaryInputArchive ar_in("/home/asellerg/data/buckets.bin");
  printf("\nLoading phmap.\n");
  cache.phmap_load(ar_in);
  printf("Loaded phmap: %ld.\n", cache.size());
}

void PureCfrMachine::do_iteration( rng_state_t &rng, int64_t num_iterations )
{
  hand_t hand;
  if( generate_hand( hand, rng ) ) {
    fprintf( stderr, "Unable to generate hand.\n" );
    exit( -1 );
  }
  for( int p = 0; p < ag.game->numPlayers; ++p ) {
    std::unordered_map<int8_t, std::vector<int8_t>> all_history;
    char history_str[1024] = {'\0'};
    uint64_t dart = genrand_int32(&rng);
    walk_pure_cfr( p, ag.betting_tree_root, hand, rng, all_history, 0, num_iterations, history_str, 0, dart );
  }
}

int PureCfrMachine::write_dump( const char *dump_prefix, intmax_t iterations_complete,
				const bool do_regrets ) const
{

  if( do_average ) {
    /* Dump avg strategy */
    try { 
      char filename[256];
      snprintf(filename, sizeof(filename), "/sda/open_pure_cfr/avg_strategy/%jd.data", iterations_complete);
      std::ofstream out(filename);
      for (auto& entry : avg_strategy_dict) {
        out << entry.first << ":";
        std::map<std::string, float> local_strategy_dict = {{"fold", 0.}, {"call", 0.}, {"raise 0.33", 0.}, {"raise 0.5", 0.}, {"raise 0.66", 0.}, {"raise 1", 0.}, {"raise 1.3", 0.}, {"raise all", 0.}};
        for (auto& sub_entry : entry.second) {
          local_strategy_dict[sub_entry.first] += sub_entry.second;
        }
        for (auto const& local_strategy : local_strategy_dict) {
          out << local_strategy.second << ",";
        }
        out << "\n";
      }
    } catch (...) {
      fprintf( stderr, "Failed to dump average strategy.\n");
    }
  }

  return 0;
}

int PureCfrMachine::load_dump( const char *dump_prefix )
{
  /* Let's load regrets first, then average strategy if necessary */

  /* Build the filename */
  char filename[ PATH_LENGTH ];
  snprintf( filename, PATH_LENGTH, "%s.regrets", dump_prefix );

  /* Open the file */
  FILE *file = fopen( filename, "r" );
  if( file == NULL ) {
    fprintf( stderr, "Could not open dump load file [%s]\n", filename );
    return 1;
  }

  /* Load regrets */
  for( int r = 0; r < ag.game->numRounds; ++r ) {

    if( regrets[ r ]->load( file ) ) {
      fprintf( stderr, "failed to load dump file [%s] for round %d\n",
	       filename, r );
      return 1;
    }
  }
  fclose( file );

  return 0;
}

int PureCfrMachine::generate_hand( hand_t &hand, rng_state_t &rng )
{
  /* First, deal out the cards and copy them over */
  State state;
  dealCards( ag.game, &rng, &state );
  memcpy( hand.board_cards, state.boardCards,
	  MAX_BOARD_CARDS * sizeof( hand.board_cards[ 0 ] ) );
  for( int p = 0; p < ag.game->numPlayers; ++p ) {
    memcpy( hand.hole_cards[ p ], state.holeCards[ p ],
	    MAX_HOLE_CARDS * sizeof( hand.hole_cards[ 0 ] ) );
  }

  /* Bucket the hands for each player, round if possible */
  if( ag.card_abs->can_precompute_buckets( ) ) {
    ag.card_abs->precompute_buckets( ag.game, hand );
  }

  /* Rank the hands */
  int ranks[ ag.game->numPlayers ];
  int top_rank = -1;
  int num_ties = 1;;
  /* State must be in the final round for rankHand to work properly */
  state.round = ag.game->numRounds - 1;
  for( int p = 0; p < ag.game->numPlayers; ++p ) {
    ranks[ p ] = rankHand( ag.game, &state, p );
    if( ranks[ p ] > top_rank ) {
      top_rank = ranks[ p ];
      num_ties = 1;
    } else if( ranks[ p ] == top_rank ) {
      ++num_ties;
    }
  }

  /* Set evaluation values */
  switch( ag.game->numPlayers ) {
  case 2:
    if( ranks[ 0 ] > ranks[ 1 ] ) {
      /* Player 0 wins in showdown */
      hand.eval.showdown_value_2p[ 0 ] = 1;
      hand.eval.showdown_value_2p[ 1 ] = -1;
    } else if( ranks[ 0 ] < ranks[ 1 ] ) {
      /* Player 1 wins in showdown */
      hand.eval.showdown_value_2p[ 0 ] = -1;
      hand.eval.showdown_value_2p[ 1 ] = 1;
    } else {
      /* Players tie in showdown */
      hand.eval.showdown_value_2p[ 0 ] = 0;
      hand.eval.showdown_value_2p[ 1 ] = 0;
    }
    break;

  case 6:
    for (int i = 0; i < (1 << 6); i++) {
      std::set<int8_t> not_folded;
      for( int p = 0; p < 6; ++p ) {
        if (i & 1 << p) {
          not_folded.insert(p);
        }
      }
      for( int p = 0; p < 6; ++p ) {
        if (!not_folded.count(p)) {
          hand.eval.pot_frac_recip[ p ][ i ] = INT_MAX;
        } else if (not_folded.size() == 1) {
          hand.eval.pot_frac_recip[ p ][ i ] = 1;
        } else {
          if (ranks[ p ] == top_rank) {
            hand.eval.pot_frac_recip[ p ][ i ] = num_ties;
          } else {
            hand.eval.pot_frac_recip[ p ][ i ] = INT_MAX;
          }
        }
      }
    }
    break;

  default:
    fprintf( stderr, "can't set terminal pot fraction denominators in "
	     "%d-player game\n", ag.game->numPlayers );
    return 1;
  }

  return 0;  
}

int PureCfrMachine::walk_pure_cfr( const int position,
				   const BettingNode *cur_node,
				   const hand_t &hand,
				   rng_state_t &rng,
           std::unordered_map<int8_t, std::vector<int8_t>> all_history,
           int8_t prev_round,
           int64_t num_iterations,
           char *history_str,
           int last_char,
	         uint64_t dart )
{
  int retval = 0;
  if( ( cur_node->get_child( ) == NULL )
      || cur_node->did_player_fold( position ) ) {
    /* Game over, calculate utility */
    retval = cur_node->evaluate( hand, position );
    return retval;
  }
  int num_active = ag.game->numPlayers;
  for (int p = 0; p < ag.game->numPlayers; p++) {
    if (cur_node->did_player_fold(p)) {
      num_active -= 1;
    }
  }
  int8_t round = cur_node->get_round( );
  if (round == 1 && num_active > 2) {
    return retval;
  }
  /* Grab some values that will be used often */
  int num_choices = cur_node->get_num_choices( );
  std::vector<uint16_t> allowed_actions;
  if (allowed_actions_cache.count(cur_node->action_mask)) {
    allowed_actions = allowed_actions_cache[cur_node->action_mask];
  } else {
    for (int x = 0; x < MAX_ABSTRACT_ACTIONS; x++) {
      if (cur_node->action_mask & (1 << x)) {
        allowed_actions.push_back(x);
      }
    }
    allowed_actions_cache[cur_node->action_mask] = allowed_actions;
  }
  int8_t player = cur_node->get_player( );
  int64_t soln_idx = cur_node->get_soln_idx( );
  auto history = all_history[round];
  int bucket;
  if( ag.card_abs->can_precompute_buckets( ) ) {
    bucket = hand.precomputed_buckets[ player ][ round ];
  } else {
    bucket = ag.card_abs->get_bucket( ag.game, cur_node, hand.board_cards,
				      hand.hole_cards, &cache);
  }
  std::string info_set = std::to_string(bucket);
  int choice;
  uint64_t pos_regrets[ num_choices ] = {0};
  uint64_t sum_pos_regrets = 0;
  sum_pos_regrets
    = regrets[ round ]->get_pos_values( bucket,
					soln_idx,
					num_choices,
					pos_regrets );
  choice = 0;
  if (round == 0) {
    info_set.append("|p|");
    for (int k = 0; k < history.size(); k++) {
      info_set.append(action_abbrevs[history[k]]);
      if (k != history.size() - 1) {
        info_set.append(",");
      }
    }
    sum_pos_regrets = 0;

    if (preflop_strategy.count(info_set)) {
      for (int j = 0; j < num_choices; j++) {
        pos_regrets[j] = preflop_strategy[info_set][allowed_actions[j]];
        sum_pos_regrets += pos_regrets[j];
      }

    } else {
      pos_regrets[0] = 1;
      sum_pos_regrets++;
    }
  }

  if( sum_pos_regrets == 0 ) {
    /* No positive regret, so assume a default uniform random current strategy. */
    for( int c = 0; c < num_choices; c++ ) {
      pos_regrets[ c ] = 1;
      sum_pos_regrets++;
    }
  }
  int32_t local_regrets[ num_choices ] = {0};
  int32_t sum_regrets = 0;
  sum_regrets = regrets[ round ]->get_local_values(
      bucket,
      soln_idx,
      num_choices,
      local_regrets);
  uint64_t dart_mod = dart % sum_pos_regrets;
  for( ; choice < num_choices; choice++ ) {
    if( dart_mod < pos_regrets[ choice ] ) {
      break;
    }
    dart_mod -= pos_regrets[ choice ];
  }
  double probs[ num_choices ] = {0.};
  for( int c = 0; c < num_choices; c++) {
    probs[ c ] = ((double) pos_regrets[ c ]) / ((double) sum_pos_regrets);
  }
  
  const BettingNode *child = cur_node->get_child( );

  int values[ num_choices ];
  std::vector<int8_t> curr;
  double vo = 0.;
  if (!all_history[round].size()) {
    history_str[last_char++] = round_abbrevs[round].c_str()[0];
    if (round == 1) {
      history_str[last_char++] = std::to_string(num_active).c_str()[0];
      history_str[last_char++] = '|';
    }
  }
  /* Update the average strategy if we are keeping track of one */
  if( do_average && round != 0 && player != position) { //  && round != 0) {
    auto key = std::to_string(bucket);
    key.append("/");
    std::string allowed_actions_str;
    if (allowed_actions_str_cache.count(cur_node->action_mask)) {
      allowed_actions_str = allowed_actions_str_cache[cur_node->action_mask];
    } else {
      for (int i = 0; i < allowed_actions.size(); i++) {
        allowed_actions_str.append(action_abbrevs[allowed_actions[i]]);
        if (i != (allowed_actions.size() - 1)) {
          allowed_actions_str.append(",");
        }
      }
      allowed_actions_str_cache[cur_node->action_mask] = allowed_actions_str;
    }
    key.append(allowed_actions_str);
    key.append(std::string(history_str));
    avg_strategy_dict[key][ordered_actions[allowed_actions[choice]]] += 1;
  }
  dart_mod = dart % 100;
  for( int c = 0; c < num_choices; ++c) {
    if ((round == 0 && pos_regrets[c] == 0) || (dart_mod >= 0 && player != position && c != choice)) {
      values[ c ] = 0;
      child = child->get_sibling( );
      continue;
    }
    curr = history;
    auto all_curr = all_history;
    curr.push_back(allowed_actions[c]);
    all_curr[round] = curr;
    char curr_history_str[1024];
    auto curr_last_char = last_char;
    memcpy(curr_history_str, history_str, 1024);
    if (all_history[round].size()) {
      curr_history_str[curr_last_char++] = ',';
    }
    auto action_str = action_abbrevs[allowed_actions[c]].c_str();
    for (int i = 0; i < strlen(action_str); i++) {
      curr_history_str[curr_last_char++] = action_str[i];
    }
    auto v = walk_pure_cfr( position, child, hand, rng, all_curr, round, num_iterations, curr_history_str, curr_last_char, dart );
    if (player != position && dart_mod >= 0) {
      vo += v;
    } else {
      vo += v * probs[ c ];
    }
    values[ c ] = v;
    child = child->get_sibling( );
  }

  retval = int(vo);

  /* Update the regrets at the current node */
  if (round != 0 && player == position) {
    regrets[ round ]->update_regret( bucket, soln_idx, num_choices,
				     values, retval );
  }
  
  return retval;
}
