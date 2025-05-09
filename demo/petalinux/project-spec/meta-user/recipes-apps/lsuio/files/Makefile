APP = lsuio

# Add any other object files to this list below
APP_OBJS = uio_helper.o lsuio.o

all: build

build: $(APP)

$(APP): $(APP_OBJS)
	$(CC)  $(LDFLAGS) -o $@ $(APP_OBJS)$(LDLIBS)
clean:
	rm -f $(APP) *.o

