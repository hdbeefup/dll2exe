CC := g++
srcdir := $(CURDIR)/../src
objdir := $(CURDIR)/../obj/linux
sources := $(wildcard $(srcdir)/**.cpp)
headers := $(wildcard $(srcdir)/**.h)
objects := $(patsubst $(srcdir)/%.cpp,$(objdir)/%.o,$(sources))
INCLUDE = \
    -I$(CURDIR)/../vendor/peframework/include \
    -I$(CURDIR)/../vendor/eirrepo \
    -I$(CURDIR)/../vendor/asmjit/src \
    -I$(CURDIR)/../vendor/asmjitshared/include
LIBDIRS = \
    -L$(CURDIR)/../vendor/peframework/lib/linux \
    -L$(CURDIR)/../vendor/asmjit/lib/linux \
    -L$(CURDIR)/../vendor/asmjitshared/lib/linux
BUILD_DIR := \
    $(CURDIR)

main : $(objects) $(headers) peframework.vendor asmjit.vendor asmjitshared.vendor ; \
    cd $(BUILD_DIR) ; \
    $(CC) $(LIBDIRS) -o ../bin/pefrmdllembed $(objects) -l peframework -l asmjit -l asmjitshared 

$(objdir)/%.o : $(srcdir)/%.cpp ; \
    mkdir -p $(dir $@) ; \
    $(CC) -o $(objdir)/$(patsubst $(objdir)/%.o,%.o,$@) -c $< -Wno-invalid-offsetof $(INCLUDE) ;

%.vendor : ; \
    cd $(BUILD_DIR)/../vendor/$(patsubst %.vendor,%,$@)/build/ ; \
    make

print-%: ; @echo '$*=$($*)'