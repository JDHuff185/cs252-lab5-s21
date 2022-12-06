#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
  char *argv[] = { "test-env", NULL };
  execvp("./http-root-dir/cgi-bin/test-env", argv);
}
