CC := g++
CCFLAGS := -std=c++17
srcdir := $(CURDIR)/../src/asmjit
objdir := $(CURDIR)/../obj/linux
sources := $(shell find $(srcdir) -name "*.cpp")
headers := $(shell find $(srcdir) -name "*.h") $(shell find $(srcdir) -name "*.hxx")
objects := $(patsubst $(srcdir)/%,$(objdir)/%.o,$(sources))
LIBMAKE = ar
INCLUDE := 
    
main : $(objects) $(headers) ; mkdir -p ../lib/linux/ ; \
    $(LIBMAKE) rcs ../lib/linux/libasmjit.a $(objects)
    
$(objdir)/%.o : $(srcdir)/% ; \
    mkdir -p $(dir $@) ; \
    $(CC) $(CCFLAGS) -O3 -o $@ -c $< $(INCLUDE) ;

clean : ; \
    rm -rf $(objdir)

print-%: ; @echo '$*=$($*)'