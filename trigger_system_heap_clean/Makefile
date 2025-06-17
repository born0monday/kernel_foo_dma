CC=gcc

SOURCE_FILES = trigger.c dma_buf_t.c
OBJ_FILES = $(patsubst %.c,%.o,$(SOURCE_FILES))

CFLAGS = -static
COBJFLAGS = $(CFLAGS) -c
LDFLAGS = 
EXEC_NAME = trigger

%.o: %.c
	$(CC) $^ $(COBJFLAGS) -o $@

$(EXEC_NAME): $(OBJ_FILES)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

run: $(EXEC_NAME)
	cp $(EXEC_NAME) ../basic_linux_env/host/$(EXEC_NAME)
	cd ../basic_linux_env && ./run_qemu.sh

clean:
	rm ./*.o
	rm $(EXEC_NAME)