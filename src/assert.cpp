#ifndef ASSERT_H
#define ASSERT_H

#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Assert and display the message
#define assert(bool, msg) \
if (bool) { \
   fprintf(stderr, "[ASSERT FAILED] %s:%d\n", __FILE__, __LINE__); \
   fprintf(stderr, "Reason: %s\n", msg); \
   abort(); \
} \

// Assert and display the message with errno string
// Exit with errno code
#define assert_err(bool, msg) \
if (bool) { \
   fprintf(stderr, "[ASSERT ERROR] %s:%d\n", __FILE__, __LINE__); \
   fprintf(stderr, "Reason: %s\n", msg); \
   fprintf(stderr, "Error: %s\n", strerror(errno)); \
   abort(); \
} \

#endif /* ifndef ASSERT_H */
