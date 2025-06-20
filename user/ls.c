#include <lib.h>

int flag[256];

int lsdir(char *, char *);
int ls1(char *, u_int, u_int, char *);

int ls(char *path, char *prefix) {
    int r;
    struct Stat st;

    if ((r = stat(path, &st)) < 0) {
        printf("ls: cannot access '%s': No such file or directory\n", path);
        return 1;
    }
    if (st.st_isdir && !flag['d']) {
        return lsdir(path, prefix);
    } else {
        return ls1(0, st.st_isdir, st.st_size, path);
    }
}

int lsdir(char *path, char *prefix) {
    int fd, n;
    struct File f;

    if ((fd = open(path, O_RDONLY)) < 0) {
        printf("ls: cannot open directory '%s'\n", path);
        return 1;
    }
    while ((n = readn(fd, &f, sizeof f)) == sizeof f) {
        if (f.f_name[0]) {
            ls1(prefix, f.f_type == FTYPE_DIR, f.f_size, f.f_name);
        }
    }
    if (n > 0) {
        printf("ls: error reading directory '%s'\n", path);
    }
    if (n < 0) {
        printf("ls: error reading directory '%s': %d\n", path, n);
    }
    close(fd);
    return 0;
}

int ls1(char *prefix, u_int isdir, u_int size, char *name) {
    char *sep;

    if (flag['l']) {
        printf("%11d %c ", size, isdir ? 'd' : '-');
    }
    if (prefix) {
        if (prefix[0] && prefix[strlen(prefix) - 1] != '/') {
            sep = "/";
        } else {
            sep = "";
        }
        printf("%s%s", prefix, sep);
    }
    printf("%s", name);
    if (flag['F'] && isdir) {
        printf("/");
    }
    printf(" ");
    return 0;
}

void usage(void) {
    printf("usage: ls [-dFl] [file...]\n");
    exit(0);
}

int main(int argc, char **argv) {
    int i;
    
    ARGBEGIN {
        default:
        usage();
        case 'd':
        case 'F':
        case 'l':
        flag[(u_char)ARGC()]++;
        break;
    }
    ARGEND
    
    int r;
    if (argc == 0) {
        char cur_path[MAXPATHLEN];
        if (syscall_get_dir(cur_path) < 0) {
            strcpy(cur_path, "/");
        }
        r = ls(cur_path, "");
    } else {
        for (i = 0; i < argc; i++) {
            r = ls(argv[i], argv[i]);
        }
    }
    printf("\n");
    exit(0);
    return r;
}