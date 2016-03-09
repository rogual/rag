rag: rag.cc
	c++ -std=c++11 -o $@ $^ -lboost_program_options -lboost_iostreams
