#include <args.h>
#include <lib.h>

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()"
#define MAX_CMDS 64
#define MAX_ARGS 128
#define MAX_VARS 16

char buf[1024];
char *argv[128];

void usage(void) {
    printf("usage: sh [-ix] [script-file]\n");
    exit(0);
}

void read_line(char *buf, u_int n);
void trim(char *s);
void remove_comments(char *s);
int run_pipeline(char *cmds[], int index, int ncmds, int input_fd);
int split_logical(char *line, char *segments[], char ops[], int *n);
int run_single_command(char *line);
int is_builtin(char *cmd);
int run_builtin(char **args);
int run_with_logic(char *line);

int sh_cd(int argc, char **args);
int sh_pwd(int argc, char **args);
int sh_declare(int argc, char **args);
int sh_unset(int argc, char **args);
int sh_exit(int argc, char **args) __attribute__((noreturn));

char *builtin_str[] = {"cd", "exit", "pwd", "declare", "unset"};

int (*builtin_func[])(int argc, char **) = {&sh_cd, &sh_exit, &sh_pwd,
                                            &sh_declare, &sh_unset};

int main(int argc, char **argv) {
    int r;
    int interactive = iscons(0);
    int echocmds = 0;
    printf("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
    printf("::                                                         ::\n");
    printf("::                     MOS Shell 2024                      ::\n");
    printf("::                                                         ::\n");
    printf(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");

    ARGBEGIN {
    case 'i':
        interactive = 1;
        break;
    case 'x':
        echocmds = 1;
        break;
    default:
        usage();
    }
    ARGEND

    if (argc > 1) {
        usage();
    }
    if (argc == 1) {
        close(0);
        if ((r = open(argv[0], O_RDONLY)) < 0) {
            user_panic("open %s: %d", argv[0], r);
        }
        user_assert(r == 0);
    }

    while (1) {
        if (interactive) {
            printf("\n$ ");
        }
        read_line(buf, sizeof buf);
        trim(buf);
        expand_vars(buf);
        remove_comments(buf);

        if (buf[0] == '\0' || buf[0] == '#') {
            continue; // 空行
        }

        if (echocmds) {
            printf("Executing: %s\n", buf);
        }

        run_with_logic(buf);
    }

    return 0;
}

void read_line(char *buf, u_int n) {
    int r;
    for (int i = 0; i < n; i++) {
        if ((r = read(0, buf + i, 1)) != 1) {
            if (r < 0) {
                debugf("read error: %d\n", r);
            }
            exit(-1);
        }
        if (buf[i] == '\b' || buf[i] == 0x7f) {
            if (i > 0) {
                i -= 2;
            } else {
                i = -1;
            }
            if (buf[i] != '\b') {
                printf("\b");
            }
        }
        if (buf[i] == '\r' || buf[i] == '\n') {
            buf[i] = 0;
            return;
        }
    }
    debugf("line too long\n");
    while ((r = read(0, buf, 1)) == 1 && buf[0] != '\r' && buf[0] != '\n') {
        ;
    }
    buf[0] = 0;
}

void trim(char *s) {
    char *end;

    // 去除前导空格
    while (*s && strchr(WHITESPACE, *s)) {
        s++;
    }

    // 去除尾部空格
    end = s + strlen(s) - 1;
    while (end > s && strchr(WHITESPACE, *end)) {
        end--;
    }
    *(end + 1) = '\0'; // 终止字符串
}

int lsh_num_builtins(void) { return sizeof(builtin_str) / sizeof(char *); }

int is_builtin(char *cmd) {
    for (int i = 0; i < lsh_num_builtins(); i++) {
        if (strcmp(cmd, builtin_str[i]) == 0) {
            //debugf("is_builtin: %s found at index %d\n", cmd, i);
            return i;
        }
    }
    return -1;
}

int run_builtin(char **args) {
    int builtin_index = is_builtin(args[0]);
    if (builtin_index == -1) {
        return -1; // 不是内置命令
    }
    // 计算参数个数
    int argc = 0;
    while (args[argc])
        argc++;

    return builtin_func[builtin_index](argc, args);
}

int chdir(const char *path) {
    if (syscall_ch_dir(path) < 0) {
        debugf("chdir %s failed\n", path);
        return -1;
    }
    return 0;
}

int sh_cd(int argc, char **args) {
    if (args[1] == NULL) {
        chdir("/");
        return 0;
    }

    if (argc > 2) {
        printf("Too many args for cd command\n");
        return 1;
    }

    char cur_path[MAXPATHLEN];
    syscall_get_dir(cur_path);
    char new_path[MAXPATHLEN];
    resolve_path(args[1], cur_path, new_path, sizeof(new_path));

    struct Stat s;
    if (stat(new_path, &s) < 0) {
        printf("cd: The directory '%s' does not exist.\n", args[1]);
        return 1;
    }

    if (!s.st_isdir) {
        printf("cd: '%s' is not a directory.\n", args[1]);
        return 1;
    }

    chdir(new_path);
    return 0;
}

int sh_pwd(int argc, char **args) {
    if (argc > 1) {
        printf("pwd: expected 0 arguments; got %d\n", argc);
        return 2;
    }

    char cur_path[MAXPATHLEN];
    if (syscall_get_dir(cur_path) < 0) {
        printf("pwd: failed to get current directory\n");
        return 1;
    }

    printf("%s\n", cur_path);
    return 0;
}

// declare [-xr] [NAME[=VALUE]]
int sh_declare(int argc, char **args) {
    int exported = 0, readonly = 0;

    //debugf("declare called with %d args\n", argc);
    int i = 1;
    while (i < argc && args[i] && args[i][0] == '-') {
        for (int j = 1; args[i][j]; ++j) {
            if (args[i][j] == 'x')
                exported = 1;
            else if (args[i][j] == 'r')
                readonly = 1;
            else {
                printf("declare: invalid option -%c\n", args[i][j]);
                return 1;
            }
        }
        i++;
    }

    if (i >= argc) {
        char names[MAX_VARS][16];
        char values[MAX_VARS][16];
        int count = syscall_all_args(names, values);
        //debugf("declare: found %d variables\n", count);
        for (int i = 0; i < count; i++) {
            printf("%s=%s\n", names[i], values[i]);
        }
    }

    while (i < argc) {
        char *eq = strchr(args[i], '=');
        if (eq) {
            *eq = '\0';
            if (syscall_set_args(args[i], eq + 1, exported, readonly) < 0) {
                return 1;
            }
            //debugf("the name is %s, value is %s, exported: %d, readonly: %d\n", args[i], eq + 1, exported, readonly);
        } else {
            if (syscall_set_args(args[i], "", exported, readonly) < 0) {
                return 1;
            }
        }
        i++;
    }
    return 0;
}

int sh_unset(int argc, char **args) {
    if (argc < 2) {
        printf("unset: expected at least 1 argument\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (syscall_unset_args(args[i]) < 0) {
            return 1;
        }
    }
    return 0;
}

__attribute__((noreturn)) int sh_exit(int argc, char **args) {
    if (argc > 1) {
        printf("exit: expected 0 arguments; got %d\n", argc);
        return 2;
    }
    close_all();
    exit(0);
}

int run_with_logic(char *line) {
    char *seg[MAX_CMDS];
    char ops[MAX_CMDS];
    int n = 0;

    split_logical(line, seg, ops, &n);

    for (int i = 0; i < n; i++) {
        int status = run_single_command(seg[i]);

        // 如果是 &&，上一条失败就停止
        if (i < n - 1 && ops[i] == '&' && status != 0)
            break;

        // 如果是 ||，上一条成功就停止
        if (i < n - 1 && ops[i] == '|' && status == 0)
            break;
    }
    return 0;
}

int split_logical(char *line, char *segments[], char ops[], int *n) {
    *n = 0;
    char *p = line;

    while (*p) {
        segments[*n] = p;
        char *next = strstr(p, "&&");
        char *alt = strstr(p, "||");
        char *sep = NULL;
        if (next && (!alt || next < alt)) {
            ops[(*n)++] = '&';
            sep = next;
        } else if (alt) {
            ops[(*n)++] = '|';
            sep = alt;
        }

        if (sep) {
            *sep = '\0'; // split string
            p = sep + 2;
        } else {
            (*n)++;
            break;
        }
    }
    return *n;
}

int run_single_command(char *line) {
    char *cmds[MAX_CMDS];
    int ncmds = 0;

    cmds[ncmds] = strtok(line, "|");
    while (cmds[ncmds] && ncmds < MAX_CMDS - 1) {
        cmds[++ncmds] = strtok(NULL, "|");
    }

    for (int i = 0; i < ncmds; ++i)
        if (cmds[i])
            while (*cmds[i] == ' ')
                ++cmds[i];

    return run_pipeline(cmds, 0, ncmds, 0);
}

void parse_args(char *line, char **args, char **infile, char **outfile) {
    int i = 0, j = 0;
    char *token = strtok(line, WHITESPACE);
    while (token && j < MAX_ARGS - 1) {
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, WHITESPACE);
            *infile = token;
        } else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, WHITESPACE);
            *outfile = token;
        } else {
            args[j++] = token;
        }
        token = strtok(NULL, WHITESPACE);
    }
    args[j] = NULL;
}

int run_pipeline(char *cmds[], int index, int ncmds, int input_fd) {
    int pipefd[2];
    char *args[MAX_ARGS];
    char *infile = NULL, *outfile = NULL;

    parse_args(cmds[index], args, &infile, &outfile);

    if (index == 0 && infile) {
        input_fd = open(infile, O_RDONLY);
        if (input_fd < 0) {
            printf("Error opening input file: %s\n", infile);
            return -1;
        }
    }

    // debugf("index: %d, ncmds: %d, input_fd: %d\n", index, ncmds, input_fd);

    // 如果是最后一个命令
    if (index == ncmds - 1) {
        int out_fd = -1;
        if (outfile) {
            out_fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC);
            if (out_fd < 0) {
                printf("Error opening output file: %s\n", outfile);
                return -1;
            }
        }

        if (is_builtin(args[0]) != -1) {
            if (input_fd != 0) {
                dup(input_fd, 0);
                close(input_fd);
            }
            if (out_fd != -1) {
                dup(out_fd, 1);
                close(out_fd);
            }
            return run_builtin(args);
        }

        int child = fork();
        if (child < 0) {
            printf("Fork failed\n");
            return -1;
        } else if (child == 0) {
            if (input_fd != 0) {
                dup(input_fd, 0);
                close(input_fd);
            }
            if (out_fd != -1) {
                dup(out_fd, 1);
                close(out_fd);
            }
            int cmd = spawn(args[0], args);
            if (cmd < 0) {
                printf("Command not found: %s\n", args[0]);
                exit(1);
            } else {
                int ret = wait(cmd);
                exit(ret);
            }
        } else {
            if (input_fd != 0) {
                close(input_fd);
            }
            if (out_fd != -1) {
                close(out_fd);
            }
            return wait(child);
        }
    }

    // 如果不是最后一个命令，需要创建管道
    pipe(pipefd);
    int child = fork();
    if (child < 0) {
        printf("Fork failed\n");
        return -1;
    } else if (child == 0) {
        if (input_fd != 0) {
            dup(input_fd, 0);
            close(input_fd);
        }
        dup(pipefd[1], 1);
        close(pipefd[0]);
        close(pipefd[1]);
        int cmd = spawn(args[0], args);
        if (cmd < 0) {
            printf("Command not found: %s\n", args[0]);
            exit(1);
        } else {
            int ret = wait(cmd);
            exit(ret);
        }
    } else {
        close(pipefd[1]);
        if (input_fd != 0) {
            close(input_fd);
        }
        return run_pipeline(cmds, index + 1, ncmds, pipefd[0]);
    }
}

void expand_vars(char *line) {
    char result[1024] = "";
    char *dest = result;

    for (char *p = line; *p;) {
        if (*p == '$') {
            p++; // 跳过 '$'
            char varname[16] = "";
            char var_value[16] = "";
            int j = 0;

            // 提取变量名
            while (*p && !strchr(WHITESPACE SYMBOLS, *p) &&
                   j < sizeof(varname) - 1) {
                varname[j++] = *p++;
            }
            varname[j] = '\0';

            if (j > 0) { // 如果有变量名
                syscall_get_args(varname, var_value);
                // 将变量值添加到结果中
                int value_len = strlen(var_value);
                if (dest - result + value_len < sizeof(result) - 1) {
                    strcpy(dest, var_value);
                    dest += value_len;
                }
            } else {
                // 如果 $ 后面没有变量名，保留 $
                if (dest - result < sizeof(result) - 1) {
                    *dest++ = '$';
                }
            }
        } else {
            // 普通字符，直接复制
            if (dest - result < sizeof(result) - 1) {
                *dest++ = *p;
            }
            p++;
        }
    }
    *dest = '\0';
    strcpy(line, result);
}

void remove_comments(char *s) {
    char *p = s;
    while (*p) {
        if (*p == '#') {
            *p = '\0'; // 注释开始，截断字符串
            break;
        }
        p++;
    }
}