# embed_spirv.cmake — Reads a .spv file and writes a C header with a uint32_t array.
# Input variables: SPV_FILE, HEADER_FILE, VAR_NAME

file(READ "${SPV_FILE}" SPV_HEX HEX)
string(LENGTH "${SPV_HEX}" SPV_HEX_LEN)
math(EXPR SPV_BYTE_COUNT "${SPV_HEX_LEN} / 2")
math(EXPR SPV_WORD_COUNT "${SPV_BYTE_COUNT} / 4")

# Convert hex pairs to 32-bit little-endian words
set(WORDS "")
set(COL 0)
math(EXPR LAST_WORD "${SPV_WORD_COUNT} - 1")
foreach(I RANGE 0 ${LAST_WORD})
  math(EXPR OFFSET "${I} * 8")
  string(SUBSTRING "${SPV_HEX}" ${OFFSET} 8 WORD_HEX)
  # Hex is stored as byte sequence; reorder to little-endian uint32_t
  string(SUBSTRING "${WORD_HEX}" 0 2 B0)
  string(SUBSTRING "${WORD_HEX}" 2 2 B1)
  string(SUBSTRING "${WORD_HEX}" 4 2 B2)
  string(SUBSTRING "${WORD_HEX}" 6 2 B3)
  set(WORD "0x${B3}${B2}${B1}${B0}")
  if(COL EQUAL 0)
    string(APPEND WORDS "\n    ")
  endif()
  string(APPEND WORDS "${WORD}")
  if(I LESS LAST_WORD)
    string(APPEND WORDS ", ")
  endif()
  math(EXPR COL "(${COL} + 1) % 6")
endforeach()

file(WRITE "${HEADER_FILE}"
"// Auto-generated from ${VAR_NAME} — do not edit.
#pragma once
#include <cstdint>
static const uint32_t ${VAR_NAME}_spv[] = {${WORDS}
};
static const size_t ${VAR_NAME}_spv_size = sizeof(${VAR_NAME}_spv);
")
