// Cflat runtime library, specialized for CS 160 Spring'24.

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#define WORDSIZE 8

// print a message to standard err and exit with a non-zero return code.
void _cflat_panic(const char * message) {
  fprintf(stderr, "%s\n", message);
  exit(1);
}

// The memory allocation function; takes the number of words to allocate and
// returns a pointer to the allocated memory, which is guaranteed to be
// zero-initialized. Uses malloc; panics if malloc returns null.
void * _cflat_alloc(size_t num_words) {
  void * p = malloc(num_words * WORDSIZE);
  if (p) {
    memset(p, 0, num_words * WORDSIZE);
    return p;
  }

  _cflat_panic("Error: out of memory.");
  return NULL;  // this is unreachable.
}

// Print the given 64-bit integer.
void printNum(int64_t n) {
  printf("%ld\n", n);
}

// Print the given 64-bit integer as a character.
void printChar(int64_t n) {
  if (n < SCHAR_MIN || n > SCHAR_MAX) {
    _cflat_panic("Error: value given to printChar is out-of-range.");
  }
  printf("%c", (char)(signed char)n);
}
