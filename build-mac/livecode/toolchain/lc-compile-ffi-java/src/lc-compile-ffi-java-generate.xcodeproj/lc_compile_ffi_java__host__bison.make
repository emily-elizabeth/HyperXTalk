all: \
    $(INTERMEDIATE_DIR)/gen.y.c

$(INTERMEDIATE_DIR)/gen.y.c \
    : \
    $(INTERMEDIATE_DIR)/gen.y
	@mkdir -p "$(INTERMEDIATE_DIR)"
	@echo note: "Yaccing \"gen.y\""
	bison -o "$(INTERMEDIATE_DIR)/gen.y.c" "$(INTERMEDIATE_DIR)/gen.y"
