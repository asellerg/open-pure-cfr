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

std::unordered_map<int, std::string> action_abbrevs = {{0, "f"}, {1, "c"}, {3, "r0.5"}, {5, "r1"}};

std::unordered_map<std::string, uint64_t[ 4 ]> preflop_strategy;

std::unordered_map<uint16_t, std::vector<uint16_t>> allowed_actions_cache;


PureCfrMachine::PureCfrMachine( const Parameters &params )
  : ag( params ),
    do_average( params.do_average )
{
  init_deck(deck);

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
        if( do_average ) {
        	switch( AVG_STRATEGY_TYPES[ r ] ) {
          	case TYPE_UINT8_T:
          	  avg_strategy[ i ][ r ]
          	    = new Entries_der<uint8_t>( num_entries_per_bucket[ r ],
          					total_num_entries[ r ] );
          	  break;

          	case TYPE_INT:
          	  avg_strategy[ i ][ r ]
          	    = new Entries_der<int>( num_entries_per_bucket[ r ],
          				    total_num_entries[ r ] );
          	  break;

            case TYPE_UINT16_T:
              avg_strategy[ i ][ r ]
                = new Entries_der<uint16_t>( num_entries_per_bucket[ r ],
                      total_num_entries[ r ] );
              break;
          	  
          	case TYPE_UINT32_T:
          	  avg_strategy[ i ][ r ]
          	    = new Entries_der<uint32_t>( num_entries_per_bucket[ r ],
          					 total_num_entries[ r ] );
          	  break;
          		
          	case TYPE_UINT64_T:
          	  avg_strategy[ i ][ r ]
          	    = new Entries_der<uint64_t>( num_entries_per_bucket[ r ],
          					 total_num_entries[ r ] );
          	  break;
          	  
          	default:
          	  fprintf( stderr, "unrecognized avg strategy type [%d]\n",
          		   AVG_STRATEGY_TYPES[ r ] );
          	  exit( -1 );
          }
        } else {
      	 avg_strategy[ i ][ r ] = NULL;
        }
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

  std::ifstream infile("/home/asellerg/open-pure-cfr/preflop_strategy.txt");
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
    for (int i = 0; i < 4; i++) {
      preflop_strategy[info_set_str][i] = std::atoi(result[i].c_str());
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
    std::vector<int8_t> history;
    walk_pure_cfr( p, ag.betting_tree_root, hand, rng, history, 0, num_iterations );
  }
}

int PureCfrMachine::write_dump( const char *dump_prefix,
				const bool do_regrets ) const
{
  /* Let's dump regrets first if required, then average strategy if necessary */
  if( do_regrets ) {
    /* Build the filename */
    char filename[ PATH_LENGTH ];
    snprintf( filename, PATH_LENGTH, "%s.regrets", dump_prefix );

    /* Open the file */
    FILE *file = fopen( filename, "w" );
    if( file == NULL ) {
      fprintf( stderr, "Could not open dump file [%s]\n", filename );
      return 1;
    }

    /* Dump regrets */
    for( int r = 0; r < ag.game->numRounds; ++r ) {
      if( regrets[ r ]->write( file ) ) {
	fprintf( stderr, "Error while dumping round %d to file [%s]\n",
		 r, filename );
	return 1;
      }
    }
    
    fclose( file );
  }

  if( do_average ) {
    /* Dump avg strategy */

    /* Build the filename */
    for (int i = 0; i < ag.game->numPlayers; i++) {
      char filename[ PATH_LENGTH ];
      snprintf( filename, PATH_LENGTH, "%s.%d.avg-strategy", dump_prefix, i );

      /* Open the file */
      FILE *file = fopen( filename, "w" );
      if( file == NULL ) {
        fprintf( stderr, "Could not open dump file [%s]\n", filename );
        return 1;
      }

      for( int r = 0; r < ag.game->numRounds; ++r ) {
        if( avg_strategy[ i ][ r ]->write( file ) ) {
  	return 1;
        }
      }

      fclose( file );
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

  if( do_average ) {
    for (int i = 0; i < ag.game->numPlayers; i++) {
      /* Build the filename */
      snprintf( filename, PATH_LENGTH, "%s.%d.avg-strategy", dump_prefix, i);

      /* Open the file */
      file = fopen( filename, "r" );
      if( file == NULL ) {
        fprintf( stderr, "WARNING: Could not open dump load file [%s]\n",
  	       filename );
        fprintf( stderr, "All average values set to zero.\n" );
        return -1;
      }

      /* Load avg strategy */
      for( int r = 0; r < ag.game->numRounds; ++r ) {
  	  
        if( avg_strategy[ i ][ r ]->load( file ) ) {
  	fprintf( stderr, "failed to load dump file [%s] for round %d\n",
  		 filename, r );
  	return 1;
        }
      }

      fclose( file );
    }
  }

  return 0;
}

int PureCfrMachine::generate_hand( hand_t &hand, rng_state_t &rng )
{
  /* First, deal out the cards and copy them over */
  State state;
  dealCards( ag.game, &rng, &state );
  memcpy( hand.board_cards, state.boardCards,
	  MAX_BOARD_CARDS * sizeof( hand.board_cards[ 0 ] ) );
  for( int p = 0; p < MAX_PURE_CFR_PLAYERS; ++p ) {
    memcpy( hand.hole_cards[ p ], state.holeCards[ p ],
	    MAX_HOLE_CARDS * sizeof( hand.hole_cards[ 0 ] ) );
  }

  /* Bucket the hands for each player, round if possible */
  if( ag.card_abs->can_precompute_buckets( ) ) {
    ag.card_abs->precompute_buckets( ag.game, hand );
  }

  /* Rank the hands */
  int ranks[ MAX_PURE_CFR_PLAYERS ];
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
    for (int i = 0; i < (1 << MAX_PURE_CFR_PLAYERS); i++) {
      std::set<int8_t> not_folded;
      for( int p = 0; p < MAX_PURE_CFR_PLAYERS; ++p ) {
        if (i & 1 << p) {
          not_folded.insert(p);
        }
      }
      for( int p = 0; p < MAX_PURE_CFR_PLAYERS; ++p ) {
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
           std::vector<int8_t> history,
           int8_t prev_round,
           int64_t num_iterations )
{
  int retval = 0;

  if( ( cur_node->get_child( ) == NULL )
      || cur_node->did_player_fold( position ) ) {
    /* Game over, calculate utility */
    
    retval = cur_node->evaluate( hand, position );
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
  int8_t round = cur_node->get_round( );
  int64_t soln_idx = cur_node->get_soln_idx( );
  if (round != prev_round) {
    history.clear();
  }
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
      std::cout << "info_set: " + info_set + "\taction_mask: " + std::to_string(cur_node->action_mask) + "\n";
    }
  }

  if( sum_pos_regrets == 0 ) {
    /* No positive regret, so assume a default uniform random current strategy */
    for( int c = 0; c < num_choices; ++c ) {
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

  /* Purify the current strategy so that we always take choice */
  uint64_t dart = genrand_int32( &rng ) % sum_pos_regrets;
  for( ; choice < num_choices; ++choice ) {
    if( dart < pos_regrets[ choice ] ) {
      break;
    }
    dart -= pos_regrets[ choice ];
  }
  double probs[ num_choices ] = {0.};
  for( int c = 0; c < num_choices; ++c) {
    probs[ c ] = ((double) pos_regrets[ c ]) / ((double) sum_pos_regrets);
  }
  
  const BettingNode *child = cur_node->get_child( );

  if( player != position ) {
    /* Opponent's node. Recurse down the single choice. */

    for( int c = 0; c < choice; ++c ) {
      child = child->get_sibling( );
    }
    history.push_back(allowed_actions[choice]);
    retval = walk_pure_cfr( position, child, hand, rng, history, round, num_iterations );

    /* Update the average strategy if we are keeping track of one */
    if( do_average && round != 0) {
      if( avg_strategy[ player ][ round ]->increment_entry( bucket, soln_idx, choice ) ) {
	fprintf( stderr, "The average strategy has overflown :(\n" );
	fprintf( stderr, "To fix this, you must set a bigger AVG_STRATEGY_TYPE "
		 "in constants.cpp and start again from scratch.\n" );
	exit( 1 );
      }
    }
    
  } else {
    /* Current player's node. Recurse down all choices to get the value of each */

    int values[ num_choices ];
    std::vector<int8_t> curr;
    double vo = 0.;
    for( int c = 0; c < num_choices; ++c) {
      if (round == 0 && pos_regrets[c] == 0) {
        values[ c ] = 0;
        child = child->get_sibling( );
        continue;
      } else if (local_regrets[c] < -300000000) {
        if (genrand_real1(&rng) < 0.95) {
          values[ c ] = 0;
          child = child->get_sibling( );
          continue;
        }
      }
      curr = history;
      curr.push_back(allowed_actions[c]);
      auto v = walk_pure_cfr( position, child, hand, rng, curr, round, num_iterations );
      vo += probs[ c ] * v;
      values[ c ] = v;
      child = child->get_sibling( );
    }

    retval = int(vo);

    /* Update the regrets at the current node */
    if (round != 0) {
      regrets[ round ]->update_regret( bucket, soln_idx, num_choices,
  				     values, retval );
    }
  }
  
  return retval;
}
