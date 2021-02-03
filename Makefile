CC=gcc
CXX=g++
RM= /bin/rm -vf
ARCH=UNDEFINED
PWD=$(shell pwd)
CDR=$(shell pwd)
ECHO=echo

EDCFLAGS:=$(CFLAGS) -I include/ -Wall -fpermissive -std=c11
EDLDFLAGS:=$(LDFLAGS) -lpthread -lm
EDDEBUG:=$(DEBUG)

ifeq ($(ARCH),UNDEFINED)
	ARCH=$(shell uname -m)
endif

UNAME_S := $(shell uname -s)

CXXFLAGS:= -I include/ -I imgui/include -I imgui/include/imgui -I imgui/include/backend -I ./ -Wall -O2 -fpermissive -std=gnu++11
LIBS = -lpthread

ifeq ($(UNAME_S), Linux) #LINUX
	ECHO_MESSAGE = "Linux"
	LIBS += -lGL `pkg-config --static --libs glfw3`
	LIBEXT= so
	LINKOPTIONS:= -shared
	CXXFLAGS += `pkg-config --cflags glfw3`
endif

ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"
	LIBEXT= dylib
	LINKOPTIONS:= -dynamiclib -single_module
	CXXFLAGS:= -arch $(ARCH) $(CXXFLAGS)
	LIBS += -arch $(ARCH) -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	LIBS += -L/usr/local/lib -L/opt/local/lib
	#LIBS += -lglfw3
	LIBS += -lglfw

	CXXFLAGS += -I/usr/local/include -I/opt/local/include
	CFLGAS+= -arch $(ARCH)
endif

all: CFLAGS+= -O2

GUITARGET=client.out

CPPOBJS=main.o

all: $(GUITARGET) imgui/libimgui_glfw.a
	$(ECHO) "Built for $(UNAME_S), execute ./$(GUITARGET)"

$(GUITARGET): $(CPPOBJS) imgui/libimgui_glfw.a
	$(CXX) $(CXXFLAGS) -o $@ $(CPPOBJS) imgui/libimgui_glfw.a $(LIBS)

imgui/libimgui_glfw.a:
	cd $(PWD)/imgui && make -j$(nproc) && cd $(PWD)

%.o: %.c
	$(CC) $(EDCFLAGS) -o $@ -c $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<

.PHONY: clean

clean:
	$(RM) $(GUITARGET)
	$(RM) $(CPPOBJS)

spotless: clean
	cd $(PWD)/imgui && make spotless && cd $(PWD)