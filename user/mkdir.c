#include <lib.h>

int create_directory_recursive(const char *path) {
    char temp_path[MAXPATHLEN];
    char *p;
    struct Stat st;

    strcpy(temp_path, path);

    // 从根开始逐级创建
    p = temp_path;
    if (*p == '/')
        p++; // 跳过根目录

    while (*p) {
        while (*p && *p != '/')
            p++;
        if (*p == '/') {
            *p = '\0';
            // 检查当前路径是否存在
            if (stat(temp_path, &st) != 0) {
                // 不存在，创建目录
                int fd = open(temp_path, O_MKDIR);
                if (fd < 0) {
                    return -1;
                }
                close(fd);
            } else if (!st.st_isdir) {
                // 存在但不是目录
                return -1;
            }
            *p = '/';
            p++;
        }
    }

    // 创建最终目录
    if (stat(temp_path, &st) != 0) {
        int fd = open(temp_path, O_MKDIR);
        if (fd < 0) {
            return -1;
        }
        close(fd);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int recursive = 0;
    char *dir_path = NULL;

    if (argc < 2) {
        printf("Usage: mkdir [-p] <directory>\n");
        exit(1); // 添加 exit
        return 1;
    }

    // 检查是否有 -p 选项
    if (strcmp(argv[1], "-p") == 0) {
        recursive = 1;
        if (argc < 3) {
            printf("mkdir: missing directory operand\n");
            exit(1); // 添加 exit
            return 1;
        }
        dir_path = argv[2];
        if (argc > 3) {
            printf("mkdir: too many arguments\n");
            exit(1); // 添加 exit
            return 1;
        }
    } else {
        dir_path = argv[1];
        if (argc > 2) {
            printf("mkdir: too many arguments\n");
            exit(1); // 添加 exit
            return 1;
        }
    }

    struct Stat st;

    // 检查目录是否已存在
    if (stat(dir_path, &st) == 0) {
        if (st.st_isdir) {
            if (recursive) {
                exit(0);  // 添加 exit
                return 0; // -p 选项时忽略已存在的目录
            } else {
                printf("mkdir: cannot create directory '%s': File exists\n",
                       dir_path);
                exit(1); // 添加 exit
                return 1;
            }
        } else {
            printf("mkdir: cannot create directory '%s': File exists\n",
                   dir_path);
            exit(1); // 添加 exit
            return 1;
        }
    }

    if (recursive) {
        // 递归创建目录
        if (create_directory_recursive(dir_path) < 0) {
            printf("mkdir: cannot create directory '%s': No such file or "
                   "directory\n",
                   dir_path);
            exit(1); // 添加 exit
            return 1;
        }
    } else {
        // 直接创建目录
        int fd = open(dir_path, O_MKDIR);
        if (fd < 0) {
            printf("mkdir: cannot create directory '%s': No such file or "
                   "directory\n",
                   dir_path);
            exit(1); // 添加 exit
            return 1;
        }
        close(fd);
    }

    exit(0); // 添加 exit
    return 0;
}