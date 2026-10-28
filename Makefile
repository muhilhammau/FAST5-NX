TARGET := fast5
PORT_NPDM_PROGRAM_ID := 0x0100000000001010

include runtime/runtime.mk

# The game's shared libraries
GAME_SO := source/lib/libFast5.so source/lib/libStormGLOFT.so

# Per-file flags if needed
# $(BUILD)/java.o: CFLAGS += -Wno-unused-variable
