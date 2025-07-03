#pragma once
#include <stdint.h>
#include "space/cell.h"

/// See hash_crypto()
#define CRYPTO_SIZE 40

/// @brief hash a regular arrow (pair)
/// @param h_tail 
/// @param h_head 
/// @return hash code
uint64_t hash_pair(uint64_t h_tail, uint64_t h_head);

/// @brief hash a string and compute its length
/// @param[in] str 
/// @param[out] length 
/// @return hash code
uint64_t hash_string(char *str, uint32_t* length);

/// @brief hash a binary string
/// @param[in] buffer 
/// @param[in] length 
/// @return hash code
uint64_t hash_raw(uint8_t *buffer, uint32_t length);

/// @brief return hChain for an arrow in a cell 
/// @param cell 
/// @return hash code
uint32_t hash_chain(Cell* cell);

/// @brief return hChildren for an arrow in a cell
/// @param cell 
/// @return 
uint32_t hash_children(Cell* cell);

/// @brief generate a crypto footprint of a blob
/// @param[in] size blob size
/// @param[in] data blob content
/// @param[out] output hexadecimal crypto hash
/// @return output
char* hash_crypto(uint32_t size, uint8_t* data, char output[CRYPTO_SIZE + 1]);

