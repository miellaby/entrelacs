#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "space/hash.h"

void test_left_rotate() {
    uint32_t value = 0x12345678;
    int shift = 4;
    uint32_t result = left_rotate(value, shift);
    printf("Left Rotate: 0x%08x rotated by %d is 0x%08x\n", value, shift, result);
}

void test_right_rotate() {
    uint32_t value = 0x12345678;
    int shift = 4;
    uint32_t result = right_rotate(value, shift);
    printf("Right Rotate: 0x%08x rotated by %d is 0x%08x\n", value, shift, result);
}

void test_hash_pair() {
    uint32_t h_tail = 0x12345678;
    uint32_t h_head = 0xFEDCBA98;
    uint32_t result = hash_pair(h_tail, h_head);
    printf("Hash Pair: 0x%08x-->0x%08x is 0x%08x\n", h_tail, h_head, result);
}

void test_hash_string() {
    char *str = "test string";
    uint32_t length;
    uint64_t result = hash_string(str, &length);
    printf("Hash String: \"%s\" has hash 0x%016lx and length %d\n", str, result, length);
}

void test_hash_raw() {
    uint8_t buffer[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t length = sizeof(buffer);
    uint64_t result = hash_raw(buffer, length);
    printf("Hash Raw: Buffer has hash 0x%016lx\n", result);
}

void test_hash_chain() {
    Cell cell;
    // Initialize cell with some test data
    cell.arrow.hash = 0x1234567;
    cell.full.type = CELLTYPE_TAG;
    cell.uint.data[1] = 0x11111111;
    cell.uint.data[2] = 0x22222222;
    uint32_t result = hash_chain(&cell);
    printf("Hash Chain: Cell has hash 0x%08x\n", result);
}

void test_hash_children() {
    Cell cell;
    // Initialize cell with some test data
    cell.arrow.hash = 0x12345678;
    cell.full.type = CELLTYPE_TAG;
    cell.uint.data[1] = 0x11111111;
    cell.uint.data[2] = 0x22222222;
    uint32_t result = hash_children(&cell);
    printf("Hash Children: Cell has hash 0x%08x\n", result);
}

void test_hash_crypto() {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t size = sizeof(data);
    char output[CRYPTO_SIZE + 1];
    char *result = hash_crypto(size, data, output);
    printf("Hash Crypto: Data has hash \"%s\" len=%lu\n", result, strlen(result));
}

int main() {
    test_left_rotate();
    test_right_rotate();
    test_hash_pair();
    test_hash_string();
    test_hash_raw();
    test_hash_chain();
    test_hash_children();
    test_hash_crypto();
    return 0;
}
