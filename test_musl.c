// Simple test program to demonstrate musl-gcc works
#include <stdio.h>
#include <string.h>

int main() {
    printf("Hello from musl libc!\n");
    printf("This binary is compiled with musl-gcc\n");

    // Show we can use standard library functions
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "String length: %zu", strlen("test"));
    printf("%s\n", buffer);

    return 0;
}
