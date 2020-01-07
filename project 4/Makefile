# CS110 Makefile Hooks: archive

PROGS = archive tptest tpcustomtest
CXX = /usr/bin/g++-5

IA_LIB_SRC = document.cc \
			 index.cc \
			 log.cc \
			 whitelist.cc

TP_LIB_SRC = thread-pool.cc

WARNINGS = -Wall -pedantic
DEPS = -MMD -MF $(@:.o=.d)
DEFINES = -D_GLIBCXX_USE_NANOSLEEP -D_GLIBCXX_USE_SCHED_YIELD
INCLUDES = -I/afs/ir/class/cs110/local/include \
		   -isystem /afs/ir/class/cs110/include/netlib \
		   -I/afs/ir/class/cs110/include/myhtml

CXXFLAGS = -g $(WARNINGS) -O0 -std=c++14 $(DEPS) $(DEFINES) $(INCLUDES)
LDFLAGS = -lm \
		  -L/afs/ir/class/cs110/lib/netlib -lcppnetlib-client-connections \
		  	  -lcppnetlib-uri -lcppnetlib-server-parsers \
		  -L/afs/ir/class/cs110/lib/myhtml -lmyhtml \
		  -L/afs/ir/class/cs110/local/lib -lrand -lthreads \
		  -pthread -lssl -lcrypto -ldl

IA_LIB_OBJ = $(patsubst %.cc,%.o,$(patsubst %.S,%.o,$(IA_LIB_SRC)))
IA_LIB_DEP = $(patsubst %.o,%.d,$(IA_LIB_OBJ))
IA_LIB = libarchive.a

TP_LIB_OBJ = $(patsubst %.cc,%.o,$(patsubst %.S,%.o,$(TP_LIB_SRC)))
TP_LIB_DEP = $(patsubst %.o,%.d,$(TP_LIB_OBJ))
TP_LIB = libthreadpool.a

PROGS_SRC = archive.cc tptest.cc tpcustomtest.cc
PROGS_OBJ = $(patsubst %.cc,%.o,$(patsubst %.S,%.o,$(PROGS_SRC)))
PROGS_DEP = $(patsubst %.o,%.d,$(PROGS_OBJ))

all: $(IA_LIB) $(PROGS)

$(PROGS): %:%.o $(IA_LIB) $(TP_LIB)
	$(CXX) $^ $(LDFLAGS) -o $@

$(GRADE_PROGS): %:%.o $(IA_LIB)
	$(CXX) $^ $(LDFLAGS) -o $@

$(IA_LIB): $(IA_LIB_OBJ)
	rm -f $@
	ar r $@ $^
	ranlib $@

$(TP_LIB): $(TP_LIB_OBJ)
	rm -f $@
	ar r $@ $^
	ranlib $@

clean:
	rm -f $(PROGS) $(PROGS_OBJ) $(PROGS_DEP)
	rm -f $(IA_LIB) $(IA_LIB_DEP) $(IA_LIB_OBJ)
	rm -f $(TP_LIB) $(TP_LIB_DEP) $(TP_LIB_OBJ)
	rm -rf indexed-documents

filefree:
	rm -rf indexed-documents

spartan: clean
	\rm -fr *~

.PHONY: all clean spartan

-include $(IA_LIB_DEP) $(TP_LIB_DEP) $(PROGS_DEP)
