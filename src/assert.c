#ifndef ASSERT_H
#define ASSERT_H

#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

// Assert on truethy value
static inline void assert(bool truethy, const char *msg) {
  if (truethy) {
    fprintf(stderr, "[ASSERT FAILED]: errno = %d\n", errno);
    perror(msg);
    exit(errno);
  }
}

#endif /* ifndef ASSERT_H */
