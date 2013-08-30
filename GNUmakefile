include $(GNUSTEP_MAKEFILES)/common.make

TOOL_NAME = catserve
catserve_C_FILES = src/catserve.c src/tcp.c src/context.c src/web.c \
		src/websockets/base64.c src/websockets/handshake.c \
		src/websockets/frames.c src/websockets/read_message.c \
		src/websockets/util.c
catserve_LDFLAGS = -llua52 -lpthread -lreadline
catserve_CFLAGS = -I/usr/local/include/lua52
catserve_INCLUDE_DIRS = -Isrc
ADDITIONAL_TOOL_LIBS = -lssl -lcrypto -lm


include $(GNUSTEP_MAKEFILES)/tool.make
