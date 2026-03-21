all: \
    $(INTERMEDIATE_DIR)/stage1/gen.l.c

$(INTERMEDIATE_DIR)/stage1/gen.l.c \
    : \
    $(INTERMEDIATE_DIR)/stage1/gen.l
	@mkdir -p "$(INTERMEDIATE_DIR)/stage1"
	@echo note: "Lexing \"gen.l\""
	flex -o "$(INTERMEDIATE_DIR)/stage1/gen.l.c" -w "$(INTERMEDIATE_DIR)/stage1/gen.l"
