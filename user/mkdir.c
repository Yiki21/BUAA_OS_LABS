#include <lib.h>

int main(int argc, char *argv[]) {
    int recursive = 0;
    char *dir_path = NULL;

    if (argc < 2) {
        write(2, "Usage: touch <filename>\n", 24);
        return 1;
    }

    if (argv[1] == NULL) {
        printf("mkdir: missing directory operand\n");
        return 1;
    }

    // // 添加调试输出
    // printf("DEBUG: argc = %d\n", argc);
    // for (int i = 0; i < argc; i++) {
    //     printf("DEBUG: argv[%d] = '%s'\n", i, argv[i] ? argv[i] : "NULL");
    // }

    // 检查是否有 -p 选项
    if (strcmp(argv[1], "-p") == 0) {
        recursive = 1;
        if (argv[2] == NULL) {
            printf("mkdir: missing directory operand\n");
            return 1;
        }
        dir_path = argv[2];
        if (argc > 3) {
            printf("mkdir: too many arguments\n");
            return 1;
        }
    } else {
        dir_path = argv[1];
        if (argc > 2) {
            printf("mkdir: too many arguments\n");
            return 1;
        }
    }

    // 直接使用传入的路径
    char *path = dir_path;
    struct Stat st;

    // 检查目录是否已存在
    if (stat(path, &st) == 0) {
        if (st.st_isdir) {
            if (recursive) {
                return 0; // -p 选项时忽略已存在的目录
            } else {
                printf("mkdir: cannot create directory '%s': File exists\n", dir_path);
                return 1;
            }
        }
    }
    
    if (recursive) {
        // 递归创建目录
        char temp_path[MAXPATHLEN];
        strcpy(temp_path, path);
        char *p = temp_path;
        
        if (*p == '/') p++; // 跳过根目录
        
        while (*p) {
            while (*p && *p != '/') p++;
            if (*p == '/') {
                *p = '\0';
                // 尝试创建当前层级的目录
                if (stat(temp_path, &st) != 0) {
                    int fd = open(temp_path, O_MKDIR);
                    if (fd < 0) {
                        printf("mkdir: cannot create directory '%s': No such file or directory\n", dir_path);
                        return 1;
                    }
                    close(fd);
                }
                *p = '/';
                p++;
            }
        }
        
        // 创建最终目录
        if (stat(path, &st) != 0) {
            int fd = open(path, O_MKDIR);
            if (fd < 0) {
                printf("mkdir: cannot create directory '%s': No such file or directory\n", dir_path);
                return 1;
            }
            close(fd);
        }
    } else {
        // 直接创建目录
        int fd = open(path, O_MKDIR);
        if (fd < 0) {
            printf("mkdir: cannot create directory '%s': No such file or directory\n", dir_path);
            return 1;
        }
        close(fd);
    }
    
    return 0;
}