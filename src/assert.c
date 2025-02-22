#ifndef ASSERT_H
#define ASSERT_H

#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Assert and display the message
// Exit with -1
void assert(bool truethy, const char *msg) {
  if (truethy) {
    fprintf(stderr, "[ASSERT FAILED]: %s\n", msg);
    exit(-1);
  }
}

// Assert and display the message with errno string
// Exit with errno code
void assert_err(bool truethy, const char *msg) {
  if (truethy) {
    fprintf(stderr, "[ASSERT ERROR]: %s\n", msg);
    fprintf(stderr, "Error: %s\n", strerror(errno));
    exit(errno);
  }
}

#endif /* ifndef ASSERT_H */
