.SUFFIXES:

TOPDIR ?= $(CURDIR)
TARGET := $(notdir $(CURDIR))
INCLUDES := includes
SOURCES := sources
BUILD := build
LIBDIRS := $(CURDIR)/lib/yaml-cpp
LIBS := -lyaml-cpp
CXXFLAGS := $(INCLUDE) -std=gnu++11 \
            -fdebug-prefix-map=$(CURDIR)=. \
            -fmacro-prefix-map=$(CURDIR)=.

LDFLAGS := -static -static-libgcc -static-libstdc++


ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
    $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

export LD := $(CXX)
export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I $(dir) ) \
    $(foreach dir,$(LIBDIRS),-I $(dir)/include) \
    -I $(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L $(dir)/lib)

.PHONY: build_cmake clean all
all: build_cmake

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
	@rm -rf Build

build_cmake:
	@echo "Deleting old 3gxtool..."
	@rm -rf $(CURDIR)/release
	@rm -f $(CURDIR)/3gxtool
	@echo "Creating build directory and running CMake..."
	@mkdir -p $(BUILD)
	@cd $(BUILD) && cmake ..
	@cd $(BUILD) && cmake --build .
	@echo "Copying compiled file and cleaning up..."
	@mkdir $(CURDIR)/release
	@cp $(BUILD)/3gxtool $(CURDIR)/release/3gxtool
	@rm -rf $(BUILD)
	@echo "Creating timestamp file..."
	@TIMESTAMP=$(shell date +"%Y-%m-%d_%H-%M-%S"); \
	echo $$TIMESTAMP > $(CURDIR)/release/$$TIMESTAMP.txt
	@echo "release directory structure created successfully."

clean:
	@echo Clean ...
	@echo "Deleting old 3gxtool..."
	@rm -rf $(CURDIR)/release
	@rm -f $(CURDIR)/3gxtool
	@rm -fr $(BUILD) $(OUTPUT)

else

DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).exe : $(OFILES)

%.o: %.cpp
	@echo $(notdir $<)
	@$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) -c $< -o $@

%.exe:
	@echo linking $(notdir $@)
	@$(LD) $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@

-include $(DEPENDS)

endif