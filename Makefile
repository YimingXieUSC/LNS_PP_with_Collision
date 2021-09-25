MYDEFS = -g -Wall -std=c++11

driver: driver.cpp
	g++ ${MYDEFS} -o driver driver.cpp

clean:
	rm -f *.o driver
