#include <lib.h>

int remove_directory_recursive(const char *path) {
    int fd, n;
    struct File f;
    char full_path[MAXPATHLEN];
    struct Stat st;

    // 打开目录
    if ((fd = open(path, O_RDONLY)) < 0) {
        return -1;
    }

    // 读取目录内容
    while ((n = readn(fd, &f, sizeof f)) == sizeof f) {
        if (f.f_name[0] && strcmp(f.f_name, ".") != 0 &&
            strcmp(f.f_name, "..") != 0) {
            // 构建完整路径
            strcpy(full_path, path);
            if (full_path[strlen(full_path) - 1] != '/') {
                strcat(full_path, "/");
            }
            strcat(full_path, f.f_name);

            if (stat(full_path, &st) == 0) {
                if (st.st_isdir) {
                    // 递归删除子目录
                    if (remove_directory_recursive(full_path) < 0) {
                        close(fd);
                        return -1;
                    }
                } else {
                    // 删除文件
                    if (remove(full_path) < 0) {
                        close(fd);
                        return -1;
                    }
                }
            }
        }
    }

    close(fd);

    // 删除空目录
    return remove(path);
}

int main(int argc, char **argv) {
    int recursive = 0;
    int force = 0;
    char *target_path = NULL;
    int arg_index = 1;

    if (argc < 2) {
        printf("Usage: rm [-rf] <file>\n");
        exit(1); // 添加 exit
        return 1;
    }

    // 解析选项
    while (arg_index < argc && argv[arg_index][0] == '-') {
        char *opt = argv[arg_index];
        if (strcmp(opt, "-r") == 0) {
            recursive = 1;
        } else if (strcmp(opt, "-rf") == 0 || strcmp(opt, "-fr") == 0) {
            recursive = 1;
            force = 1;
        } else if (strcmp(opt, "-f") == 0) {
            force = 1;
        } else {
            printf("rm: invalid option '%s'\n", opt);
            exit(1); // 添加 exit
            return 1;
        }
        arg_index++;
    }

    if (arg_index >= argc) {
        printf("rm: missing operand\n");
        exit(1); // 添加 exit
        return 1;
    }

    if (arg_index + 1 < argc) {
        printf("rm: too many arguments\n");
        exit(1); // 添加 exit
        return 1;
    }

    target_path = argv[arg_index];
    struct Stat st;

    // 检查文件/目录是否存在
    if (stat(target_path, &st) < 0) {
        if (!force) {
            printf("rm: cannot remove '%s': No such file or directory\n",
                   target_path);
            exit(1); // 添加 exit
            return 1;
        }
        exit(0);  // 添加 exit
        return 0; // -f 选项时忽略不存在的文件
    }

    // 如果是目录但没有 -r 选项
    if (st.st_isdir && !recursive) {
        printf("rm: cannot remove '%s': Is a directory\n", target_path);
        exit(1); // 添加 exit
        return 1;
    }

    // 删除文件或目录
    int result;
    if (st.st_isdir && recursive) {
        result = remove_directory_recursive(target_path);
    } else {
        result = remove(target_path);
    }

    if (result < 0) {
        if (!force) {
            printf("rm: cannot remove '%s': No such file or directory\n",
                   target_path);
            exit(1); // 添加 exit
            return 1;
        }
    }

    exit(0); // 添加 exit
    return 0;
}