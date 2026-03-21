all: \
    $(INTERMEDIATE_DIR)/stage1/gen.y.c

$(INTERMEDIATE_DIR)/stage1/gen.y.c \
    : \
    $(INTERMEDIATE_DIR)/stage1/gen.y
	@mkdir -p "$(INTERMEDIATE_DIR)/stage1"
	@echo note: "Yaccing \"gen.y\""
	bison -o "$(INTERMEDIATE_DIR)/stage1/gen.y.c" "$(INTERMEDIATE_DIR)/stage1/gen.y"
