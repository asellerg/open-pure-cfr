/* betting_node.cpp
 * Richard Gibson, Jun 28, 2013
 *
 * Constructors and evaluation methods for betting nodes.
 *
 * Copyright (C) 2013 by Richard Gibson
 */

/* C / C++ / STL includes */
#include <string.h>

/* Pure CFR includes */
#include "betting_node.hpp"

BettingNode::BettingNode( )
{
  sibling = NULL;
}

BettingNode::~BettingNode( )
{
}

TerminalNode2p::TerminalNode2p( const bool new_showdown,
				const int8_t new_fold_value[ 2 ],
				const int new_money )
  : BettingNode( ),
    showdown( new_showdown ),
    money( new_money )
{
  fold_value[ 0 ] = new_fold_value[ 0 ];
  fold_value[ 1 ] = new_fold_value[ 1 ];
}

TerminalNode2p::~TerminalNode2p( )
{
}

int TerminalNode2p::evaluate( const hand_t &hand, const int position ) const
{
  return ( showdown ? hand.eval.showdown_value_2p[ position ]
	   : fold_value[ position ] ) * money;
}

InfoSetNode2p::InfoSetNode2p( const int64_t new_soln_idx,
			      const int new_num_choices,
			      const int8_t new_player,
			      const int8_t new_round,
			      const BettingNode *new_child,
            uint16_t new_action_mask )
  : BettingNode( ),
    soln_idx( new_soln_idx ),
    num_choices( new_num_choices ),
    player( new_player ),
    round( new_round ),
    child( new_child )
{
    action_mask = new_action_mask;
}

InfoSetNode2p::~InfoSetNode2p( )
{
}

TerminalNode6p::TerminalNode6p(
				const uint16_t new_money_spent[ MAX_PURE_CFR_PLAYERS ],
				const leaf_type_t new_leaf_type )
  : BettingNode( ),
    leaf_type( new_leaf_type )
{
  memcpy( money_spent, new_money_spent, MAX_PURE_CFR_PLAYERS * sizeof( money_spent[ 0 ] ) );
}

TerminalNode6p::~TerminalNode6p( )
{
}

int TerminalNode6p::evaluate( const hand_t &hand, const int position ) const
{
  uint16_t pot_size = 0;
  for (int i = 0; i < 6; i++) {
    pot_size += money_spent[i];
  }
  // Rake.
  pot_size -= std::min(12, int(0.05 * pot_size));
  auto pot_share = ( pot_size / hand.eval.pot_frac_recip[ position ][ leaf_type ] )
    - money_spent[ position ];
  return pot_share;
}

InfoSetNode6p::InfoSetNode6p( const int64_t new_soln_idx,
			      const int new_num_choices,
			      const int8_t new_player,
			      const int8_t new_round,
			      const int8_t new_player_folded
			      [ MAX_PURE_CFR_PLAYERS ],
			      const BettingNode *new_child,
			      const uint16_t new_money_spent
			      [ MAX_PURE_CFR_PLAYERS ],
			      const leaf_type_t new_leaf_type,
            uint16_t new_action_mask )
  : TerminalNode6p( new_money_spent, new_leaf_type ),
    soln_idx( new_soln_idx ),
    num_choices( new_num_choices ),
    player( new_player ),
    round( new_round ),
    child( new_child )
{
  memcpy( player_folded, new_player_folded, MAX_PURE_CFR_PLAYERS * sizeof( player_folded[ 0 ] ) );
  action_mask = new_action_mask;
}

InfoSetNode6p::~InfoSetNode6p( )
{
}

void get_term_values_6p( const State &state,
			 const Game *game,
			 leaf_type_t *leaf_type )
{
  int8_t leaf_type_int = 63;
  for( int p = 0; p < game->numPlayers; ++p ) {
    if (state.playerFolded[ p ]) {
      leaf_type_int &= ~(1 << p);
    }
  }

  *leaf_type = static_cast<leaf_type_t>(leaf_type_int);
}

BettingNode *init_betting_tree_r( State &state,
				  const Game *game,
				  const ActionAbstraction *action_abs,
				  size_t num_entries_per_bucket[ MAX_ROUNDS ],
          std::map<uint16_t, BettingNode*> (*roots) [ MAX_ROUNDS ][ MAX_ABSTRACT_ACTIONS ] )
{
  BettingNode *node;
  
  if( state.finished ) {
    /* Terminal node */
    switch( game->numPlayers ) {
      
    case 2: {
      int8_t showdown = ( state.playerFolded[ 0 ]
			  || state.playerFolded[ 1 ] ? 0 : 1 );
      int8_t fold_value[ 2 ];
      int money = -1;
      for( int p = 0; p < 2; ++p ) {
        if( state.playerFolded[ p ] ) {
	  fold_value[ p ] = -1;
	  money = state.spent[ p ];
	} else if( state.playerFolded[ !p ] ) {
	  fold_value[ p ] = 1;
	  money = state.spent[ !p ];
	} else {
	  fold_value[ p ] = 0;
	  money = state.spent[ p ];
	}
      }
      node = new TerminalNode2p( showdown, fold_value, money );
      break;
    }

    case 6: {
      uint16_t money_spent[ 6 ];
      for (int p = 0; p < 6; p++) {
        money_spent[ p ] = state.spent[ p ];
      }
      leaf_type_t leaf_type;
      get_term_values_6p( state, game, &leaf_type );
      node = new TerminalNode6p( money_spent, leaf_type );
      break;
    }
    
    default:
      fprintf( stderr, "cannot initialize betting tree for %d-players\n",
	       game->numPlayers );
      assert( 0 );
    }
    
    return node;
  }

  /* Choice node.  First, compute number of different allowable actions */
  Action actions[ MAX_ABSTRACT_ACTIONS ];
  uint16_t action_mask = 0;
  int num_choices = action_abs->get_actions( game, state, actions, &action_mask );

  /* Next, grab the index for this node into the regrets and avg_strategy */
  int64_t soln_idx = num_entries_per_bucket[ state.round ];

  /* Update number of entries */
  num_entries_per_bucket[ state.round ] += num_choices;
  
  /* Recurse to create children */
  BettingNode *first_child = NULL;
  BettingNode *last_child = NULL;


  // assert(num_folds < 5);

  for( int a = 0; a < num_choices; ++a ) {
    BettingNode *child;
    State new_state( state );
    doAction( game, &actions[ a ], &new_state );
    child = init_betting_tree_r( new_state, game, action_abs,
					     num_entries_per_bucket, roots );
    if( last_child != NULL ) {
      last_child->set_sibling( child );
    } else {
      first_child = child;
    }
    last_child = child;
  }
  assert( first_child != NULL );
  assert( last_child != NULL );

  /* Siblings are represented by a linked list,
   * so the last child should have no sibling
   */
  last_child->set_sibling( NULL );

  /* Create the InfoSetNode */
  switch( game->numPlayers ) {
  case 2:
    node = new InfoSetNode2p( soln_idx, num_choices,
			      currentPlayer( game, &state ),
			      state.round, first_child, action_mask ); 
    break;

  case 6:
    /* We need some additional values not needed in 2p games */
    int8_t player_folded[ 6 ];
    for( int p = 0; p < game->numPlayers; ++p ) {
      player_folded[ p ] = ( state.playerFolded[ p ] ? 1 : 0 );
    }
    uint16_t money_spent[ 6 ];
    leaf_type_t leaf_type;
    get_term_values_6p( state, game, &leaf_type );
    
    node = new InfoSetNode6p( soln_idx, num_choices, currentPlayer( game, &state ),
			      state.round, player_folded, first_child,
			      money_spent, leaf_type, action_mask );
    break;

  default:
    fprintf( stderr, "cannot initialize betting tree for %d-players\n",
	     game->numPlayers );
    assert( 0 );
  }

  return node;
}

void destroy_betting_tree_r( const BettingNode *node )
{
  const BettingNode *child = node->get_child( );
  
  while( child != NULL ) {
    const BettingNode *old_child = child;
    child = child->get_sibling( );
    destroy_betting_tree_r( old_child );
  }
  
  delete node;
}
