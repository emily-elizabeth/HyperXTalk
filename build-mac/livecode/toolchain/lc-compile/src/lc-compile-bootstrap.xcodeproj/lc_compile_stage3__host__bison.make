all: \
    $(INTERMEDIATE_DIR)/stage3/gen.y.c

$(INTERMEDIATE_DIR)/stage3/gen.y.c \
    : \
    $(INTERMEDIATE_DIR)/stage3/gen.y
	@mkdir -p "$(INTERMEDIATE_DIR)/stage3"
	@echo note: "Yaccing \"gen.y\""
	bison -o "$(INTERMEDIATE_DIR)/stage3/gen.y.c" "$(INTERMEDIATE_DIR)/stage3/gen.y"
