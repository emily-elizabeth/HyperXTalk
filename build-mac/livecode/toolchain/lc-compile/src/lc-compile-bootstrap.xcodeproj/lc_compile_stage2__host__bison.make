all: \
    $(INTERMEDIATE_DIR)/stage2/gen.y.c

$(INTERMEDIATE_DIR)/stage2/gen.y.c \
    : \
    $(INTERMEDIATE_DIR)/stage2/gen.y
	@mkdir -p "$(INTERMEDIATE_DIR)/stage2"
	@echo note: "Yaccing \"gen.y\""
	bison -o "$(INTERMEDIATE_DIR)/stage2/gen.y.c" "$(INTERMEDIATE_DIR)/stage2/gen.y"
