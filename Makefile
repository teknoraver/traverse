CFLAGS := -g -Wall -O2 -pipe
BIN := fts nftw traverse

all:: $(BIN)

clean::
	$(RM) -r $(BIN) *.o test/

traverse: traverse_internal.o

comp:: all
	./fts .
	./nftw .
	./traverse .
	./fts -s .
	./nftw -s .
	./traverse -s .

testsetup::
	$(RM) -r test
	mkdir -p test
	touch test/a test/b test/c test/d test/1 test/2 test/3 test/a9 test/a10 test/a11 test/a10b test/a10b9 test/a10b10 test/a11b test/.config
	mkdir -p test/size
	dd status=none if=/dev/zero of=test/size/a4k bs=4k count=1
	dd status=none if=/dev/zero of=test/size/b0k bs=1 count=0
	dd status=none if=/dev/zero of=test/size/c16k bs=16k count=1
	dd status=none if=/dev/zero of=test/size/d8k bs=8k count=1
	dd status=none if=/dev/zero of=test/size/e10k bs=10k count=1
	mkdir -p test/dira test/dirb test/dirc test/.local
	touch test/dira/f test/dirb/f test/dirc/f

traverse_test: traverse_internal.o

test:: testsetup traverse_test
	./traverse_test test
