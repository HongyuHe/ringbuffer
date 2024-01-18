.phony: compile lock spin notify optimized tail yield check local single all clean

compile: src/main.cpp include/*.hpp
	g++ src/main.cpp -Iinclude -std=c++11 -lpthread -o rb

lock: compile
	./rb 0 lock

spin: compile
	./rb 0 spin

notify: compile
	./rb 0 notify

optimized:
	g++ -DMEM_RELAXED src/main.cpp -Iinclude -std=c++11 -lpthread -o rb
	./rb 0 optimized

yield:
	g++ -DMEM_RELAXED src/main.cpp -Iinclude -std=c++11 -lpthread -o rb
	./rb 0 yield

tail:
	g++ -DMEM_RELAXED src/main.cpp -Iinclude -std=c++11 -lpthread -o rb
	./rb 0 tail

check: 
	g++ -DMEM_RELAXED src/main.cpp -Iinclude -std=c++11 -lpthread -o rb
	strace -c -f ./rb 1 optimized

local:
	g++ -DARM -DMEM_RELAXED src/main.cpp -Iinclude -std=c++11 -lpthread -o rb
	./rb 0 optimized

single: compile
	g++ src/single.cpp -Iinclude -std=c++11 -lpthread -o rb
	./rb 0 single

all: single lock spin notify tail yield optimized

clean:
	rm rb