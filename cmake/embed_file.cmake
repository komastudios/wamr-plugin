# embed_file.cmake — Generate a C header embedding a binary file as a uint8_t array.
#
# Called via:
#   cmake -DINPUT_FILE=<path> -DOUTPUT_FILE=<path> -DVAR_NAME=<name> -P embed_file.cmake
#
# Produces a header with:
#   static const uint8_t <VAR_NAME>_data[] = { 0x00, 0x61, ... };
#   static const size_t <VAR_NAME>_size = <byte count>;

file(READ "${INPUT_FILE}" hex HEX)

string(LENGTH "${hex}" hex_length)
math(EXPR byte_count "${hex_length} / 2")

# Convert hex pairs to "0x##, " format
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " hex "${hex}")

file(WRITE "${OUTPUT_FILE}"
    "/* Auto-generated — do not edit. */\n"
    "#pragma once\n"
    "#include <stddef.h>\n"
    "#include <stdint.h>\n\n"
    "static const uint8_t ${VAR_NAME}_data[] = {\n"
    "    ${hex}\n"
    "};\n\n"
    "static const size_t ${VAR_NAME}_size = ${byte_count};\n"
)
