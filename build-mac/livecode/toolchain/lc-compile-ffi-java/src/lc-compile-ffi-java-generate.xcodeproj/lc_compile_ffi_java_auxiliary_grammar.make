all: \
    $(INTERMEDIATE_DIR)/bind.c \
    $(INTERMEDIATE_DIR)/check.c \
    $(INTERMEDIATE_DIR)/output.c \
    $(INTERMEDIATE_DIR)/generate.c \
    $(INTERMEDIATE_DIR)/support.c \
    $(INTERMEDIATE_DIR)/types.c

$(INTERMEDIATE_DIR)/bind.c \
    : \
    bind.g \
    $(INTERMEDIATE_DIR)/gen.lit \
    $(INTERMEDIATE_DIR)/gen.tkn
	@mkdir -p "$(INTERMEDIATE_DIR)"
	@echo note: "Running gentle on auxiliary grammar \"bind.g\""
	"$(BUILT_PRODUCTS_DIR)/gentle-target" -inputdir . "bind.c=$(INTERMEDIATE_DIR)/bind.c" "bind.g"

$(INTERMEDIATE_DIR)/check.c \
    : \
    check.g \
    $(INTERMEDIATE_DIR)/gen.lit \
    $(INTERMEDIATE_DIR)/gen.tkn
	@mkdir -p "$(INTERMEDIATE_DIR)"
	@echo note: "Running gentle on auxiliary grammar \"check.g\""
	"$(BUILT_PRODUCTS_DIR)/gentle-target" -inputdir . "check.c=$(INTERMEDIATE_DIR)/check.c" "check.g"

$(INTERMEDIATE_DIR)/output.c \
    : \
    output.g \
    $(INTERMEDIATE_DIR)/gen.lit \
    $(INTERMEDIATE_DIR)/gen.tkn
	@mkdir -p "$(INTERMEDIATE_DIR)"
	@echo note: "Running gentle on auxiliary grammar \"output.g\""
	"$(BUILT_PRODUCTS_DIR)/gentle-target" -inputdir . "output.c=$(INTERMEDIATE_DIR)/output.c" "output.g"

$(INTERMEDIATE_DIR)/generate.c \
    : \
    generate.g \
    $(INTERMEDIATE_DIR)/gen.lit \
    $(INTERMEDIATE_DIR)/gen.tkn
	@mkdir -p "$(INTERMEDIATE_DIR)"
	@echo note: "Running gentle on auxiliary grammar \"generate.g\""
	"$(BUILT_PRODUCTS_DIR)/gentle-target" -inputdir . "generate.c=$(INTERMEDIATE_DIR)/generate.c" "generate.g"

$(INTERMEDIATE_DIR)/support.c \
    : \
    support.g \
    $(INTERMEDIATE_DIR)/gen.lit \
    $(INTERMEDIATE_DIR)/gen.tkn
	@mkdir -p "$(INTERMEDIATE_DIR)"
	@echo note: "Running gentle on auxiliary grammar \"support.g\""
	"$(BUILT_PRODUCTS_DIR)/gentle-target" -inputdir . "support.c=$(INTERMEDIATE_DIR)/support.c" "support.g"

$(INTERMEDIATE_DIR)/types.c \
    : \
    types.g \
    $(INTERMEDIATE_DIR)/gen.lit \
    $(INTERMEDIATE_DIR)/gen.tkn
	@mkdir -p "$(INTERMEDIATE_DIR)"
	@echo note: "Running gentle on auxiliary grammar \"types.g\""
	"$(BUILT_PRODUCTS_DIR)/gentle-target" -inputdir . "types.c=$(INTERMEDIATE_DIR)/types.c" "types.g"
