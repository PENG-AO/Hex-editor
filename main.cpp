#include <cstdio>
#include "editor.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("no enough arguments\n");
        exit(1);
    }

    Editor(argv[1]).run();

    return 0;
}
