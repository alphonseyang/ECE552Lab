# Description: Makefile for building a cbp submission.

CFLAGS = -g -o3 -Wall
CXXFLAGS = -g -o3 -Wall

objects = tracer.o predictor.o main.o 

predictor : $(objects)
	$(CXX) -o $@ $(objects)

mb: mb.c
	$(CXX) -g -o0 $@.c -o $@
	/cad2/ece552f/pin/run_branchtrace $(CURDIR)/mb

clean :
	rm -f predictor $(objects)

