BIN_DIR = $(shell pwd)/src/build/bin

ferret:
	$(MAKE) -C ./src lock PORR=1

base:
	$(MAKE) -C ./src lock
	mv $(BIN_DIR)/ferret-lock $(BIN_DIR)/ferret-lock_base

clean:
	$(MAKE) -C ./src clean
	rm -rf ./src/build/obj ./src/build/lib
