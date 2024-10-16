APP := test_argus

CC = gcc
CPP = g++
CPPFLAGS = -fno-omit-frame-pointer -fsanitize=address -O1 -ggdb
LDFLAGS = -fno-omit-frame-pointer -fsanitize=address

SRCS = main.cpp

OBJS = $(SRCS:.cpp=.o)

CPPFLAGS += \
	-I/usr/src/jetson_multimedia_api/argus/include \
	-I/usr/local/cuda-12.6/targets/aarch64-linux/include

LDFLAGS += \
	-L /usr/lib/aarch64-linux-gnu/nvidia \
	-lnvargus -lcuda

all: $(APP)

$(CLASS_DIR)/%.o: $(CLASS_DIR)/%.cpp
	$(AT)$(MAKE) -C $(CLASS_DIR)

%.o: %.cpp
	@echo "Compiling: $<"
	$(CPP) $(CPPFLAGS) -c $< -o $@

$(APP): $(OBJS)
	@echo "Linking: $@"
	$(CPP) -o $@ $(OBJS) $(CPPFLAGS) $(LDFLAGS)

clean:
	$(AT)rm -rf $(APP) $(OBJS)
