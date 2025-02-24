#ifndef ASSERT_H
#define ASSERT_H

#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Assert and display the message
// Exit with -1
#define assert(bool, msg) { \
  if (bool) { \
    fprintf(stderr, "[ASSERT FAILED] %s:%d\n", __FILE__, __LINE__); \
    fprintf(stderr, "Reason: %s\n", msg); \
    exit(-1); \
  } \
}

// Assert and display the message with errno string
// Exit with errno code
#define assert_err(bool, msg) { \
  if (bool) { \
    fprintf(stderr, "[ASSERT ERROR] %s:%d\n", __FILE__, __LINE__); \
    fprintf(stderr, "Reason: %s\n", msg); \
    fprintf(stderr, "Error: %s\n", strerror(errno)); \
    exit(-1); \
  } \
}

#endif /* ifndef ASSERT_H */
