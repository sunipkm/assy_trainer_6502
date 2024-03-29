CC=gcc
CXX=g++
RM= /bin/rm -vf
ARCH=UNDEFINED
PWD=$(shell pwd)
CDR=$(shell pwd)
ECHO=echo

EDCFLAGS:=$(CFLAGS) -I include/ -I mos6502/ -Wall -std=gnu11
EDLDFLAGS:=$(LDFLAGS) -lpthread -lm
EDDEBUG:=$(DEBUG)

ifeq ($(ARCH),UNDEFINED)
	ARCH=$(shell uname -m)
endif

UNAME_S := $(shell uname -s)

CXXFLAGS:= -I include/ -I imgui/include -I clkgen/include -I mos6502/ -Wall -O2 -fpermissive -std=c++11
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
	LIBS += `pkg-config --static --libs glfw3`

	CXXFLAGS += -I/usr/local/include -I/opt/local/include `pkg-config --cflags glfw3`
	CFLGAS+= -arch $(ARCH)
endif

all: CFLAGS+= -O2

GUITARGET=mos6502.out

COBJS=mos6502/c_6502.o

CPPOBJS=main.o ImGuiFileDialog.o

all: $(GUITARGET) imgui/libimgui_glfw.a clkgen/libclkgen.a
	@$(ECHO) "Built for $(UNAME_S), execute ./$(GUITARGET)"

$(GUITARGET): $(CPPOBJS) $(COBJS) imgui/libimgui_glfw.a clkgen/libclkgen.a
	@$(CXX) $(CXXFLAGS) -o $@ $(CPPOBJS) $(COBJS) imgui/libimgui_glfw.a clkgen/libclkgen.a $(LIBS)

imgui/libimgui_glfw.a:
	@cd $(PWD)/imgui && make -j$(nproc) && cd $(PWD)

clkgen/libclkgen.a:
	@cd $(PWD)/clkgen && make -j$(nproc) && cd $(PWD)

%.o: %.c
	@$(CC) $(EDCFLAGS) -o $@ -c $<

%.o: %.cpp
	@$(CXX) $(CXXFLAGS) -o $@ -c $<

.PHONY: clean

clean:
	@$(RM) $(GUITARGET)
	@$(RM) $(CPPOBJS)
	@$(RM) $(COBJS)
	@$(RM) clkgen/libclkgen.a

spotless: clean
	@cd $(PWD)/imgui && make spotless && cd $(PWD)
	@cd $(PWD)/clkgen && make clean && cd $(PWD)
