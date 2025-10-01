CFLAGS := -g -Wall -O2 -pipe
BIN := fts nftw traverse

all:: $(BIN)

test:: all
	./fts .
	./nftw .
	./traverse .
	./fts -s .
	./nftw -s .
	./traverse -s .
