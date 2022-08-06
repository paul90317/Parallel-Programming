all:
	g++ h4_problem1.cpp -o h4_problem1.out -lpthread
	g++ Smooth.cpp -o Smooth.out
	./h4_problem1.out 16
	time ./Smooth.out
	diff output.bmp output2.bmp
	