MYDEFS = -g -Wall -std=c++11 -DLOCALHOST=\"127.0.0.1\"

all: echo-server echo-client

pa2: pa2.cpp my_socket.cpp my_socket.h my_readwrite.cpp my_readwrite.h my_timestamp.cpp my_timestamp.h
	g++ ${MYDEFS} -o pa2 pa2.cpp my_socket.cpp my_readwrite.cpp my_timestamp.cpp -lcrypto
	
echo-server: echo-server.cpp my_socket.cpp my_socket.h
	g++ ${MYDEFS} -o echo-server echo-server.cpp my_socket.cpp

echo-client: echo-client.cpp my_socket.cpp my_socket.h
	g++ ${MYDEFS} -o echo-client echo-client.cpp my_socket.cpp

run-server:
	./echo-server 12345

run-client:
	./echo-client 12345

lab3c: lab3c.cpp my_socket.cpp my_socket.h
	g++ ${MYDEFS} -o lab3c lab3c.cpp my_socket.cpp

lab3d: lab3d.cpp my_socket.cpp my_socket.h my_readwrite.cpp my_readwrite.h
	g++ ${MYDEFS} -o lab3d lab3d.cpp my_socket.cpp my_readwrite.cpp

lab4a: lab4a.cpp my_socket.cpp my_socket.h my_readwrite.cpp my_readwrite.h
	g++ ${MYDEFS} -o lab4a lab4a.cpp my_socket.cpp my_readwrite.cpp

pa3: pa3.cpp my_socket.cpp my_socket.h my_readwrite.cpp my_readwrite.h my_timestamp.cpp my_timestamp.h
	g++ ${MYDEFS} -o pa3 pa3.cpp my_socket.cpp my_readwrite.cpp my_timestamp.cpp -lcrypto -pthread

lab8b: lab8b.cpp my_socket.cpp my_socket.h my_readwrite.cpp my_readwrite.h my_timestamp.cpp my_timestamp.h
	g++ ${MYDEFS} -o lab8b lab8b.cpp my_socket.cpp my_readwrite.cpp my_timestamp.cpp -lcrypto -pthread

clean:
	rm -f *.o echo-server echo-client lab3c lab3d lab4a pa2 pa3

