#include <lib.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        write(2, "Usage: touch <file>\n", 20);
        return 1;
    }
    if (argv[2] != NULL) {
        printf("touch: too many arguments\n");
        return 1;
    }

    // 直接使用传入的路径，不需要 resolve_path
    char *path = argv[1];
    struct Stat st;
    
    // 检查文件是否已存在
    if (stat(path, &st) == 0) {
        return 0; // 文件存在，正常退出
    }
    
    // 尝试创建文件
    int fd = open(path, O_CREAT | O_WRONLY);
    if (fd < 0) {
        printf("touch: cannot touch '%s': No such file or directory\n", argv[1]);
        return 1;
    }
    close(fd);
    return 0;
}