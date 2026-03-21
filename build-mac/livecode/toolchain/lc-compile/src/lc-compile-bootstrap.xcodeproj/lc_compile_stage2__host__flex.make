all: \
    $(INTERMEDIATE_DIR)/stage2/gen.l.c

$(INTERMEDIATE_DIR)/stage2/gen.l.c \
    : \
    $(INTERMEDIATE_DIR)/stage2/gen.l
	@mkdir -p "$(INTERMEDIATE_DIR)/stage2"
	@echo note: "Lexing \"gen.l\""
	flex -o "$(INTERMEDIATE_DIR)/stage2/gen.l.c" -w "$(INTERMEDIATE_DIR)/stage2/gen.l"
