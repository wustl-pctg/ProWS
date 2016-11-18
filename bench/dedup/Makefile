BIN_DIR = $(shell pwd)/src/cilk

dedup:
	$(MAKE) -C ./src/cilk lock PORR=1

base:
	$(MAKE) -C ./src/cilk lock
	mv $(BIN_DIR)/dedup-lock $(BIN_DIR)/dedup-lock_base

clean:
	$(MAKE) -C ./src/cilk clean
