#include <lib.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: touch <file>\n");
        return 1;
    }
    
    if (argc > 2) {
        printf("touch: too many arguments\n");
        return 1;
    }

    char *file_path = argv[1];
    struct Stat st;
    
    // 检查文件是否已存在
    if (stat(file_path, &st) == 0) {
        return 0; // 文件存在，正常退出
    }
    
    // 尝试创建文件
    int fd = open(file_path, O_CREAT | O_WRONLY);
    if (fd < 0) {
        printf("touch: cannot touch '%s': No such file or directory\n", file_path);
        return 1;
    }
    close(fd);
    return 0;
}