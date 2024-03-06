/* card_abstraction.cpp
 * Richard Gibson, Jun 28, 2013
 *
 * Home of the card_abstraction abstract class and all implementing classes
 *
 * Copyright (C) 2013 by Richard Gibson
 */

/* C / C++ / STL indluces */

/* project_acpc_server includes */
extern "C" {
}

/* Pure CFR includes */
#include "card_abstraction.hpp"
#include <algorithm>
#include <sw/redis++/redis++.h>
#include "parallel_hashmap/phmap.h"
#include "constants.hpp"

using namespace sw::redis;

CardAbstraction::CardAbstraction( )
{
}

CardAbstraction::~CardAbstraction( )
{
}

/* By default, assume cannot precompute buckets */
void CardAbstraction::precompute_buckets( const Game *game, hand_t &hand ) const
{
  fprintf( stderr, "precompute_buckets called for base "
	   "card abstraction class!\n" );
  assert( false );
}

NullCardAbstraction::NullCardAbstraction( const Game *game )
  : deck_size( game->numSuits * game->numRanks )
{
  /* Precompute number of buckets per round */
  m_num_buckets[ 0 ] = 1;
  for( int i = 0; i < game->numHoleCards; ++i ) {
    m_num_buckets[ 0 ] *= deck_size;
  }
  for( int r = 0; r < MAX_ROUNDS; ++r ) {
    if( r < game->numRounds ) {
      if( r > 0 ) {
	m_num_buckets[ r ] = m_num_buckets[ r - 1 ];
      }
      for( int i = 0; i < game->numBoardCards[ r ]; ++i ) {
	m_num_buckets[ r ] *= deck_size;
      }
    } else {
      m_num_buckets[ r ] = 0;
    }
  }
}

NullCardAbstraction::~NullCardAbstraction( )
{
}

int NullCardAbstraction::num_buckets( const Game *game,
				      const BettingNode *node ) const
{
  return m_num_buckets[ node->get_round() ];
}

int NullCardAbstraction::num_buckets( const Game *game,
				      const State &state ) const
{
  return m_num_buckets[ state.round ];
}

int NullCardAbstraction::get_bucket( const Game *game,
				     const BettingNode *node,
				     const uint8_t board_cards[ MAX_BOARD_CARDS ],
				     const uint8_t hole_cards
				     [ MAX_PURE_CFR_PLAYERS ]
				     [ MAX_HOLE_CARDS ],
             hash_t *cache,
             sw::redis::Redis *redis) const
{
  return get_bucket_internal( game, board_cards, hole_cards,
			      node->get_player(), node->get_round() );
}

void NullCardAbstraction::precompute_buckets( const Game *game,
					      hand_t &hand ) const
{
  for( int p = 0; p < game->numPlayers; ++p ) {
    for( int r = 0; r < game->numRounds; ++r ) {
      hand.precomputed_buckets[ p ][ r ] = get_bucket_internal( game,
								hand.board_cards,
								hand.hole_cards,
								p, r );
    }
  }
}

int NullCardAbstraction::get_bucket_internal( const Game *game,
					      const uint8_t board_cards
					      [ MAX_BOARD_CARDS ],
					      const uint8_t hole_cards
					      [ MAX_PURE_CFR_PLAYERS ]
					      [ MAX_HOLE_CARDS ],
					      const int player,
					      const int round ) const
{
  /* Calculate the unique bucket number for this hand */
  int bucket = 0;
  for( int i = 0; i < game->numHoleCards; ++i ) {
    if( i > 0 ) {
      bucket *= deck_size;
    }
    uint8_t card = hole_cards[ player ][ i ];
    bucket += rankOfCard( card ) * game->numSuits + suitOfCard( card );
  }
  for( int r = 0; r <= round; ++r ) {
    for( int i = bcStart( game, r ); i < sumBoardCards( game, r ); ++i ) {
      bucket *= deck_size;
      uint8_t card = board_cards[ i ];
      bucket += rankOfCard( card ) * game->numSuits + suitOfCard( card );
    }
  }

  return bucket;
}

PotentialAwareImperfectRecallAbstraction::PotentialAwareImperfectRecallAbstraction ( )
{
}

PotentialAwareImperfectRecallAbstraction::~PotentialAwareImperfectRecallAbstraction ( )
{
}

int PotentialAwareImperfectRecallAbstraction::num_buckets( const Game *game,
               const BettingNode *node ) const
{
  switch(node->get_round()) {
    case 0:
      return 170;
    case 1:
      return 501;
    case 2:
      return 201;
    case 3:
      return 201;
  }
  return 1;
}

int PotentialAwareImperfectRecallAbstraction::num_buckets( const Game *game,
               const State &state ) const
{
  switch(state.round) {
    case 0:
      return 170;
    case 1:
      return 501;
    case 2:
      return 201;
    case 3:
      return 201;
  }
  return 1;
}

int compare_card_reverse(void const *a, void const *b) {
    uint8_t aa = *(uint8_t *)a;
    uint8_t bb = *(uint8_t *)b;

    if (aa > bb) return -1;
    if (aa < bb) return 1;
    return 0;
}

int compare_card(void const *a, void const *b) {
    uint8_t aa = *(uint8_t *)a;
    uint8_t bb = *(uint8_t *)b;

    if (aa > bb) return 1;
    if (aa < bb) return -1;
    return 0;
}

void sort_cards(const uint8_t* cards, int num_cards, uint64_t *hand_int, bool reverse = true) {

    for (int i = 0; i < num_cards; i++) {
      hand_int[i] = (13 * (cards[i] % 4)) + (cards[i] / 4) + 1;
    }
    if (reverse) {
      std::qsort(hand_int, num_cards, sizeof(uint64_t), compare_card_reverse);
    } else {
      std::qsort(hand_int, num_cards, sizeof(uint64_t), compare_card);
    }
}


int PotentialAwareImperfectRecallAbstraction::get_bucket( const Game *game,
              const BettingNode *node,
              const uint8_t board_cards
              [ MAX_BOARD_CARDS ],
              const uint8_t hole_cards
              [ MAX_PURE_CFR_PLAYERS ]
              [ MAX_HOLE_CARDS ],
              hash_t *cache,
              sw::redis::Redis *redis) const
{
  uint64_t sorted_hole_cards[2];
  sort_cards(hole_cards[node->get_player()], 2, sorted_hole_cards, true);
  uint64_t idx = 0;
  if (node->get_round() == 0) {
    idx = (sorted_hole_cards[0]) | (sorted_hole_cards[1] << 8);
    if (!cache->count(idx)) {
      printf("Missing idx: %d for hole cards %d %d.\n", idx, sorted_hole_cards[0], sorted_hole_cards[1]);
      return 0;
    }
    uint16_t bucket = (*cache)[idx];
    return bucket;
  }
  uint64_t sorted_board_cards[5] = {0};
  if (node->get_round() == 1) {
    sort_cards(board_cards, 3, sorted_board_cards, false);
    idx = (sorted_hole_cards[0]) | (sorted_hole_cards[1] << 8) | (sorted_board_cards[0] << 16) | (sorted_board_cards[1] << 24) | (sorted_board_cards[2] << 32);
  } else if (node->get_round() == 2) {
    sort_cards(board_cards, 4, sorted_board_cards, false);
    idx = (sorted_hole_cards[0]) | (sorted_hole_cards[1] << 8) | (sorted_board_cards[0] << 16) | (sorted_board_cards[1] << 24) | (sorted_board_cards[2] << 32) | (sorted_board_cards[3] << 40);
  } else if (node->get_round() == 3) {
    sort_cards(board_cards, 5, sorted_board_cards, false);
    idx = (sorted_hole_cards[0]) | (sorted_hole_cards[1] << 8) | (sorted_board_cards[0] << 16) | (sorted_board_cards[1] << 24) | (sorted_board_cards[2] << 32) | (sorted_board_cards[3] << 40) | (sorted_board_cards[4] << 48);
  }
  int count = cache->count(idx);
  if (!count) {
    printf("Missing idx: %d for board %d %d %d %d %d %d %d.\n", idx, sorted_hole_cards[0], sorted_hole_cards[1], sorted_board_cards[0], sorted_board_cards[1], sorted_board_cards[2], sorted_board_cards[3], sorted_board_cards[4]);
    return 0;
  }
  return (*cache)[idx];
}

BlindCardAbstraction::BlindCardAbstraction( )
{
}

BlindCardAbstraction::~BlindCardAbstraction( )
{
}

int BlindCardAbstraction::num_buckets( const Game *game,
				       const BettingNode *node ) const
{
  return 1;
}

int BlindCardAbstraction::num_buckets( const Game *game,
				       const State &state ) const
{
  return 1;
}

int BlindCardAbstraction::get_bucket( const Game *game,
				      const BettingNode *node,
				      const uint8_t board_cards
				      [ MAX_BOARD_CARDS ],
				      const uint8_t hole_cards
				      [ MAX_PURE_CFR_PLAYERS ]
				      [ MAX_HOLE_CARDS ],
              hash_t *cache,
              sw::redis::Redis *redis) const
{
  return 0;
}

void BlindCardAbstraction::precompute_buckets( const Game *game, hand_t &hand ) const
{
  for( int p = 0; p < game->numPlayers; ++p ) {
    for( int r = 0; r < game->numRounds; ++r ) {
      hand.precomputed_buckets[ p ][ r ] = 0;
    }
  }
}
