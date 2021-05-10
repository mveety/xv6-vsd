#include <libc.h>

int
main(int argc, char *argv[])
{
	int number = 1234;
	char *string = "Hello world!";

	char *str1 = sprintf("this is a number int hex %x, and decimal %d", number, number);
	char *str2 = sprintf("this is a string: \"%s\"", string);

	printf(1, "str1 = \"%s\"\n", str1);
	printf(1, "str2 = \"%s\"\n", str2);
	exit();
}
