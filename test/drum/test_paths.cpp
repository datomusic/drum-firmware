#include <cstdio>
int main() {
    FILE* f = fopen("/sequencer_state.dat", "wb");
    if (f) {
        printf("Root path works\n");
        fclose(f);
    } else {
        printf("Root path failed\n");
    }
    
    f = fopen("./sequencer_state.dat", "wb");
    if (f) {
        printf("Local path works\n");
        fclose(f);
    } else {
        printf("Local path failed\n");
    }
    return 0;
}
