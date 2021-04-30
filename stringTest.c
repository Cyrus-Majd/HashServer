#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
	char *stringOne = "hello\n";
	char stringTwo[999];
	printf("thing! %c\n", stringOne[strlen(stringOne)-1]);
	strncpy(stringTwo, stringOne, strlen(stringOne)-1);
	printf("thing2: %s\n", stringTwo);
}
