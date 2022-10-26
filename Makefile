ifeq ($(HOSTTYPE),)
	HOSTTYPE := $(shell uname -m)_$(shell uname -s)
endif

Name = libft_malloc.so
FullName = libft_malloc_$(HOSTTYPE).so
Srcs = malloc.c
Objs = $(addprefix bin/, $(Srcs:.c=.o))
Header = malloc.h

Incs = -I.
Flag = $(Incs)
Flag += -Wall -Wextra -Werror -g
Flag += -O3

Red = \033[1;91m
Green = \033[1;92m
Eoc = \033[0m

all: $(Name)

bin/%.o: src/%.c $(Header)
	@echo -e "🔧 $(Green)Compiling$(Eoc) | $(notdir $<)"
	@mkdir -p bin
	@gcc $(Flag) -fPIC $< -o $@ -c

$(Name): $(Objs)
	@echo -e "📦 $(Green)Archiving$(Eoc) | $(FullName)"
	@gcc $(Flag) $^ -shared -o $(FullName)
	@echo -e "🔗 $(Green)Linking$(Eoc)   | $(Name) -> $(FullName)"
	@rm -rf $(Name)
	@ln -s $(FullName) $(Name)

clean:
	@echo -e "🗑  $(Red)Removing$(Eoc)  | bin"
	@rm -rf bin

fclean: clean
	@echo -e "🗑  $(Red)Removing$(Eoc)  | $(Name)"
	@rm -rf $(Name) $(FullName) test

re: fclean all

test: all
	@echo -e "🎯 $(Green)Compiling$(Eoc) | $@"
	@gcc $(Flag) mytest.c -o $@ -L. -lft_malloc
	@echo -e "🖥  $(Green)Launching$(Eoc)  | $@"
	@export LD_LIBRARY_PATH=. && ./$@

.PHONY: all clean fclean re test