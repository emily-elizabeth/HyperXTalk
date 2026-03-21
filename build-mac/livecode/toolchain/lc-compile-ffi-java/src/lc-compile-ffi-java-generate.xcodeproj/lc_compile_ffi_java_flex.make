all: \
    $(INTERMEDIATE_DIR)/gen.l.c

$(INTERMEDIATE_DIR)/gen.l.c \
    : \
    $(INTERMEDIATE_DIR)/gen.l
	@mkdir -p "$(INTERMEDIATE_DIR)"
	@echo note: "Lexing \"gen.l\""
	flex -o "$(INTERMEDIATE_DIR)/gen.l.c" -w "$(INTERMEDIATE_DIR)/gen.l"
