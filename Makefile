CFLAGS = -Wall -Wno-unused-function

tte: src/tte.cpp
	g++ $(CFLAGS) -o $@ $<

clean:
	$(RM) tte
