#include <cstdio>
int main() {
    FILE* f = fopen("/tmp/test.dat", "wb");
    if (f) {
        printf("File created successfully\n");
        fclose(f);
        return 0;
    } else {
        printf("Failed to create file\n");
        return 1;
    }
}
