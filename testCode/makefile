.PHONY: replay stat clean
replay:
	gcc replay.cpp -lstdc++ -o replay -g

stat:
	gcc stat.cpp -lstdc++ -lcrypto -o stat -g

clean: 
	rm a.out
	rm replay
	rm stat