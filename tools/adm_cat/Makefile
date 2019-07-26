CXXFLAGS=-Wall -O0 -g -std=c++11 -I../../include

OBJECTS := $(patsubst %.C,%.o,$(wildcard *.C))
INCLUDES := $(notdir $(wildcard ../../include/*.h))

vpath %.h ../../include

.PHONY: all clean

all: adm_cat

$(OBJECTS): $(INCLUDES)

adm_cat: $(OBJECTS)
	$(CXX) -o $@ $(OBJECTS) -lz

clean:
	$(RM) cat_adm $(OBJECTS)
