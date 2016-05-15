# Makefile for `Magic Target' program

mtarget:
	gcc -o mtarget mtarget.c -lncurses -lm

.PHONY: clean rebuild
build: mtarget
rebuild: clean build
clean:
	-rm -f mtarget
