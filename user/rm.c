#include <lib.h>

int main(int argc, char **argv) {
    int recursive = 0;
    int force = 0;
    char *target_path = NULL;
    int arg_index = 1;
    
    if (argc < 2) {
        write(2, "Usage: rm [-rf] <file>\n", 23);
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
            return 1;
        }
        arg_index++;
    }

    if (arg_index >= argc) {
        printf("rm: missing operand\n");
        return 1;
    }
    
    if (arg_index + 1 < argc) {
        printf("rm: too many arguments\n");
        return 1;
    }
    
    target_path = argv[arg_index];
    
    // 直接使用传入的路径
    char *path = target_path;
    struct Stat st;
    
    // 检查文件/目录是否存在
    if (stat(path, &st) < 0) {
        if (!force) {
            printf("rm: cannot remove '%s': No such file or directory\n", target_path);
            return 1;
        }
        return 0; // -f 选项时忽略不存在的文件
    }
    
    // 如果是目录但没有 -r 选项
    if (st.st_isdir && !recursive) {
        printf("rm: cannot remove '%s': Is a directory\n", target_path);
        return 1;
    }
    
    // 删除文件或目录
    if (remove(path) < 0) {
        if (!force) {
            printf("rm: cannot remove '%s': No such file or directory\n", target_path);
            return 1;
        }
    }
    
    return 0;
}