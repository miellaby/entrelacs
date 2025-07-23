#pragma once
#include <stdint.h>
#include "space/cell.h"

/// See hash_crypto()
#define CRYPTO_SIZE 40

// helpers
uint32_t left_rotate(uint32_t value, int shift);
uint32_t right_rotate(uint32_t value, int shift);

/// @brief hash of XL_EVE
uint32_t hash_eve();

/// @brief hash a regular arrow (pair)
/// @param h_tail
/// @param h_head
/// @return hash code
uint32_t hash_pair(uint32_t h_tail, uint32_t h_head);

/// @brief hash a string and compute its length
/// @param[in] str
/// @param[out] length
/// @return hash code
uint64_t hash_string(const char *str, uint32_t* length);

/// @brief hash a binary string
/// @param[in] buffer
/// @param[in] length
/// @return hash code
uint64_t hash_raw(const uint8_t *buffer, const uint32_t length);

/// @brief return hChain for an arrow in a cell
/// @param cell
/// @return hash code
uint32_t hash_chain(Cell* cell);

/// @brief return offset for children for an arrow in a cell
/// @param cell
/// @return
uint32_t hash_children(Cell* cell);

/// @brief generate a crypto footprint of a blob
/// @param[in] size blob size
/// @param[in] data blob content
/// @param[out] output hexadecimal crypto hash
/// @return output
char* hash_crypto(const uint32_t size, const uint8_t* data, char output[CRYPTO_SIZE + 1]);
