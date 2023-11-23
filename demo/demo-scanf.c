#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  char name[10]; // who has a name longer than 9 chars anyway?
  printf("What is your name?\n");
  scanf("%s", name);

  printf("Hello user %s!\n", name);

  return 0;
}
