CXXFLAGS=-Wall -fPIC -O0 -g -I../include -std=c++11 -fno-inline
#CXXFLAGS=-Wall -fPIC -O2 -g -I../include -std=c++11
LDFLAGS=-shared -ldl -lunwind -lz -ldl -lpthread

OBJECTS := $(patsubst %.C,%.o,$(wildcard *.C))
INCLUDES := $(notdir $(wildcard ../include/*.h))

vpath %.h ../include

.PHONY: all clean

all: libadm.so libpadm.so

$(OBJECTS): $(INCLUDES)

libadm.so: $(OBJECTS)
	$(CXX) -o $@ $(OBJECTS) $(LDFLAGS)

modules/adm_perf.o: modules/adm_perf.C
	$(CXX) $(CXXFLAGS) -o $@ -c $< -DEVENT=0x81d0 -DPERIOD=1000

libpadm.so: modules/adm_perf.o $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $<

clean:
	$(RM) libadm.so libpadm.so modules/adm_perf.o $(OBJECTS)
