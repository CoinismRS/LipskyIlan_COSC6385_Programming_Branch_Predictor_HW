CXX		=	g++
CXXFLAGS	=	-g -O3 -Wall

all:		predict predict_extra_credit

predict:	predict.cc trace.cc predictor.h branch.h trace.h my_predictor.h
		$(CXX) $(CXXFLAGS) -o predict predict.cc trace.cc

predict_extra_credit:        predict_extra_credit.cc trace.cc predictor.h branch.h trace.h my_predictor.h
		$(CXX) $(CXXFLAGS) -o predict_extra_credit predict_extra_credit.cc trace.cc

# design-space experiment driver used for REPORT.md
experiments:	experiments.cc trace.cc predictor.h branch.h trace.h my_predictor.h
		$(CXX) $(CXXFLAGS) -o experiments experiments.cc trace.cc

# replays the worked example from the assignment handout
test_example:	../test/test_example.cc predictor.h branch.h my_predictor.h
		$(CXX) $(CXXFLAGS) -I. -o test_example ../test/test_example.cc

test_loop:	../test/test_loop.cc predictor.h branch.h my_predictor.h
		$(CXX) $(CXXFLAGS) -I. -o test_loop ../test/test_loop.cc

test:		test_example test_loop
		./test_example
		./test_loop

clean:
		rm -f predict predict_extra_credit experiments test_example test_loop

.PHONY:		all test clean
