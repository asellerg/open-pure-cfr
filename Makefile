#
# Makefile for Open Pure CFR
# Richard Gibson, June 26, 2013
#

OPT = -Wall -O3 -ffast-math -funroll-all-loops -ftree-vectorize -DHAVE_MMAP -I/home/asellerg/XPokerEval/XPokerEval.CactusKev/ -L/home/asellerg/XPokerEval/XPokerEval.CactusKev/ -L/usr/local/redis-plus-plus/ -I/usr/local/redis-plus-plus/include/ -I/home/asellerg/miniconda3/envs/epicac/include -L/home/asellerg/miniconda3/envs/epicac/lib -std=c++17 -static-libstdc++ 

PURE_CFR_FILES = pure_cfr.o acpc_server_code/game.o acpc_server_code/rng.o constants.o parameters.o utility.o card_abstraction.o action_abstraction.o betting_node.o entries.o abstract_game.o player_module.o pure_cfr_machine.o /usr/local/redis-plus-plus/lib/libredis++.a /usr/local/lib/libhiredis.a /home/asellerg/XPokerEval/XPokerEval.CactusKev/mtrand.o /home/asellerg/XPokerEval/XPokerEval.CactusKev/pokerlib.o

PRINT_PLAYER_STRATEGY_FILES = player_module.o print_player_strategy.o acpc_server_code/game.o acpc_server_code/rng.o constants.o parameters.o utility.o card_abstraction.o action_abstraction.o betting_node.o entries.o abstract_game.o /usr/local/redis-plus-plus/lib/libredis++.a /usr/local/lib/libhiredis.a

PURE_CFR_PLAYER_FILES = pure_cfr_player.o player_module.o acpc_server_code/game.o acpc_server_code/rng.o acpc_server_code/net.o constants.o parameters.o utility.o card_abstraction.o action_abstraction.o betting_node.o entries.o abstract_game.o /usr/local/redis-plus-plus/lib/libredis++.a /usr/local/lib/libhiredis.a

BUCKETS_FILES = buckets.cpp /usr/local/redis-plus-plus/lib/libredis++.a /usr/local/lib/libhiredis.a /home/asellerg/XPokerEval/XPokerEval.CactusKev/mtrand.o /home/asellerg/XPokerEval/XPokerEval.CactusKev/pokerlib.o

BUCKETS_SPANNER_FILES = buckets_spanner.cpp /home/asellerg/miniconda3/envs/epicac/lib/libgoogle_cloud* /home/asellerg/miniconda3/envs/epicac/lib/libproto* /usr/local/redis-plus-plus/lib/libredis++.a /usr/local/lib/libhiredis.a /home/asellerg/XPokerEval/XPokerEval.CactusKev/mtrand.o /home/asellerg/XPokerEval/XPokerEval.CactusKev/pokerlib.o

BUCKETS_TEXT_FILES = buckets_text.cpp /home/asellerg/miniconda3/envs/epicac/lib/libgoogle_cloud* /home/asellerg/miniconda3/envs/epicac/lib/libproto* /usr/local/redis-plus-plus/lib/libredis++.a /usr/local/lib/libhiredis.a /home/asellerg/XPokerEval/XPokerEval.CactusKev/mtrand.o /home/asellerg/XPokerEval/XPokerEval.CactusKev/pokerlib.o

WRITE_CLUSTERS_FILES = write_clusters.cpp /usr/local/redis-plus-plus/lib/libredis++.a /usr/local/lib/libhiredis.a

LOOKUP_BUCKETS_FILES = lookup_buckets.cpp


all: pure_cfr print_player_strategy pure_cfr_player buckets write_clusters

%.o: %.cpp
	$(CXX) $(OPT) -c $^

pure_cfr: $(PURE_CFR_FILES)
	$(CXX) $(OPT) -pthread -o $@ $(PURE_CFR_FILES)

print_player_strategy: $(PRINT_PLAYER_STRATEGY_FILES)
	$(CXX) $(OPT) -ltbb -o $@ $(PRINT_PLAYER_STRATEGY_FILES)

pure_cfr_player: $(PURE_CFR_PLAYER_FILES)
	$(CXX) $(OPT) -ltbb -o $@ $(PURE_CFR_PLAYER_FILES)

buckets: $(BUCKETS_FILES)
	$(CXX) $(OPT) -pthread -lhiredis -lredis++ -mcmodel=medium -o $@ $(BUCKETS_FILES)

buckets_spanner: $(BUCKETS_SPANNER_FILES)
	$(CXX) $(OPT) -pthread -lhiredis -lredis++ -mcmodel=medium -o $@ $(BUCKETS_SPANNER_FILES)

buckets_text: $(BUCKETS_TEXT_FILES)
	$(CXX) $(OPT) -pthread -lhiredis -lredis++ -mcmodel=medium -o $@ $(BUCKETS_TEXT_FILES)

write_clusters: $(WRITE_CLUSTERS_FILES)
	$(CXX) $(OPT) -pthread -lhiredis -lredis++ -mcmodel=medium -o $@ $(WRITE_CLUSTERS_FILES)

lookup_buckets: $(LOOKUP_BUCKETS_FILES)
	$(CXX) $(OPT) -fPIC -shared -pthread -o lookup_buckets.so $(LOOKUP_BUCKETS_FILES)

clean: 
	-rm *.o acpc_server_code/*.o
	-rm pure_cfr print_player_strategy pure_cfr_player buckets write_clusters
