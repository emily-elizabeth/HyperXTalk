all: \
    $(INTERMEDIATE_DIR)/stage2/bind.c \
    $(INTERMEDIATE_DIR)/stage2/check.c \
    $(INTERMEDIATE_DIR)/stage2/generate.c \
    $(INTERMEDIATE_DIR)/stage2/support.c \
    $(INTERMEDIATE_DIR)/stage2/syntax.c \
    $(INTERMEDIATE_DIR)/stage2/types.c

$(INTERMEDIATE_DIR)/stage2/bind.c \
    : \
    bind.g \
    $(INTERMEDIATE_DIR)/stage2/gen.lit \
    $(INTERMEDIATE_DIR)/stage2/gen.tkn
	@mkdir -p "$(INTERMEDIATE_DIR)/stage2"
	@echo note: "Running gentle on auxiliary grammar \"bind.g\""
	"$(BUILT_PRODUCTS_DIR)/gentle-target" -inputdir . "bind.c=$(INTERMEDIATE_DIR)/stage2/bind.c" "bind.g"

$(INTERMEDIATE_DIR)/stage2/check.c \
    : \
    check.g \
    $(INTERMEDIATE_DIR)/stage2/gen.lit \
    $(INTERMEDIATE_DIR)/stage2/gen.tkn
	@mkdir -p "$(INTERMEDIATE_DIR)/stage2"
	@echo note: "Running gentle on auxiliary grammar \"check.g\""
	"$(BUILT_PRODUCTS_DIR)/gentle-target" -inputdir . "check.c=$(INTERMEDIATE_DIR)/stage2/check.c" "check.g"

$(INTERMEDIATE_DIR)/stage2/generate.c \
    : \
    generate.g \
    $(INTERMEDIATE_DIR)/stage2/gen.lit \
    $(INTERMEDIATE_DIR)/stage2/gen.tkn
	@mkdir -p "$(INTERMEDIATE_DIR)/stage2"
	@echo note: "Running gentle on auxiliary grammar \"generate.g\""
	"$(BUILT_PRODUCTS_DIR)/gentle-target" -inputdir . "generate.c=$(INTERMEDIATE_DIR)/stage2/generate.c" "generate.g"

$(INTERMEDIATE_DIR)/stage2/support.c \
    : \
    support.g \
    $(INTERMEDIATE_DIR)/stage2/gen.lit \
    $(INTERMEDIATE_DIR)/stage2/gen.tkn
	@mkdir -p "$(INTERMEDIATE_DIR)/stage2"
	@echo note: "Running gentle on auxiliary grammar \"support.g\""
	"$(BUILT_PRODUCTS_DIR)/gentle-target" -inputdir . "support.c=$(INTERMEDIATE_DIR)/stage2/support.c" "support.g"

$(INTERMEDIATE_DIR)/stage2/syntax.c \
    : \
    syntax.g \
    $(INTERMEDIATE_DIR)/stage2/gen.lit \
    $(INTERMEDIATE_DIR)/stage2/gen.tkn
	@mkdir -p "$(INTERMEDIATE_DIR)/stage2"
	@echo note: "Running gentle on auxiliary grammar \"syntax.g\""
	"$(BUILT_PRODUCTS_DIR)/gentle-target" -inputdir . "syntax.c=$(INTERMEDIATE_DIR)/stage2/syntax.c" "syntax.g"

$(INTERMEDIATE_DIR)/stage2/types.c \
    : \
    types.g \
    $(INTERMEDIATE_DIR)/stage2/gen.lit \
    $(INTERMEDIATE_DIR)/stage2/gen.tkn
	@mkdir -p "$(INTERMEDIATE_DIR)/stage2"
	@echo note: "Running gentle on auxiliary grammar \"types.g\""
	"$(BUILT_PRODUCTS_DIR)/gentle-target" -inputdir . "types.c=$(INTERMEDIATE_DIR)/stage2/types.c" "types.g"
