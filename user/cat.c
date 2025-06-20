#include <lib.h>

char buf[8192];

void cat(int f, char *s) {
    long n;
    int r;

    while ((n = read(f, buf, (long)sizeof buf)) > 0) {
        if ((r = write(1, buf, n)) != n) {
            user_panic("write error copying %s: %d", s, r);
        }
    }
    if (n < 0) {
        user_panic("error reading %s: %d", s, n);
    }
}

int main(int argc, char **argv) {
    int f, i;
    int overall_status = 0; // 记录整体状态

    if (argc == 1) {
        cat(0, "<stdin>");
    } else {
        // printf("cat: ");
        for (i = 1; i < argc; i++) {
            f = open(argv[i], O_RDONLY);
            if (f < 0) {
                printf("cat: can't open %s: %d\n", argv[i], f);
                overall_status = 1; // 记录错误
            } else {
                cat(f, argv[i]);
                close(f);
            }
        }
    }
    exit(overall_status); // 添加 exit 并使用正确状态
    return overall_status;
}