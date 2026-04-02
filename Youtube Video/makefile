CXX := g++
TARGET := yt_run
SRC := yt_run.cpp

CUDA_DIR := /usr/local/cuda-10.2

OPENCV_FLAGS := $(shell pkg-config --cflags --libs opencv4)

CXXFLAGS := -std=c++14 -O3 \
-I$(CUDA_DIR)/include \
-I/usr/include/aarch64-linux-gnu

LDFLAGS := \
-L$(CUDA_DIR)/targets/aarch64-linux/lib \
-lnvinfer \
-lcuda \
-lcudart \
$(OPENCV_FLAGS)

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(SRC) -o $(TARGET) $(CXXFLAGS) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)