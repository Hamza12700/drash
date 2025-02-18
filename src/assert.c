#ifndef ASSERT_H
#define ASSERT_H

#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Assert on truethy value
void assert(bool truethy, const char *msg) {
  if (truethy) {
    fprintf(stderr, "[ASSERT FAILED]: %s\n", msg);
    fprintf(stderr, "Error: %s\n", strerror(errno));
    exit(errno);
  }
}

#endif /* ifndef ASSERT_H */
