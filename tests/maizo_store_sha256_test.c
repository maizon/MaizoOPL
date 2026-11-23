#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "maizo_store_sha256.h"

static void test(const char *message, const char *expected)
{
    maizo_sha256_t s;
    char result[65];
    size_t n = strlen(message);
    maizo_sha256_init(&s);
    for (size_t i = 0; i < n; i++)
        maizo_sha256_update(&s, message + i, 1);
    maizo_sha256_final(&s, result);
    assert(strcmp(result, expected) == 0);
}
int main(void)
{
    test("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    test("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    test("The quick brown fox jumps over the lazy dog",
         "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
    puts("MAIZO Store SHA-256 vectors OK");
    return 0;
}
