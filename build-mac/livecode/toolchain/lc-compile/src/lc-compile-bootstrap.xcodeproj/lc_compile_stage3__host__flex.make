all: \
    $(INTERMEDIATE_DIR)/stage3/gen.l.c

$(INTERMEDIATE_DIR)/stage3/gen.l.c \
    : \
    $(INTERMEDIATE_DIR)/stage3/gen.l
	@mkdir -p "$(INTERMEDIATE_DIR)/stage3"
	@echo note: "Lexing \"gen.l\""
	flex -o "$(INTERMEDIATE_DIR)/stage3/gen.l.c" -w "$(INTERMEDIATE_DIR)/stage3/gen.l"
