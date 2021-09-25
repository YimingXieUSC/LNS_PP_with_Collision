MYDEFS = -g -Wall -std=c++14 

driver: driver.cpp
	g++ ${MYDEFS} -o driver driver.cpp

a_star: a_star.cpp
	g++ ${MYDEFS} -o a_star a_star.cpp

clean:
	rm -f *.o driver
