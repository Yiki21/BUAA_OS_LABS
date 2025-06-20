#include <args.h>
#include <lib.h>

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()"
#define MAX_CMDS 64
#define MAX_ARGS 128
#define MAX_VARS 16
#define HISTFILESIZE 20

char buf[1024];
char *argv[128];
int history_fd = -1;
int history_index = 0;

// 添加历史记录相关的全局变量
char history_lines[HISTFILESIZE][1024]; // 内存中的历史记录
int history_count = 0;                  // 当前历史记录数量
int history_current = -1;               // 当前浏览的历史位置
char current_input[1024];               // 保存当前正在输入的内容

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

void store_history(char *buf);
void load_history(void);         // 新增：加载历史记录
void save_history_to_file(void); // 新增：保存历史记录到文件

int sh_cd(int argc, char **args);
int sh_pwd(int argc, char **args);
int sh_declare(int argc, char **args);
int sh_unset(int argc, char **args);
int sh_exit(int argc, char **args) __attribute__((noreturn));
int sh_history(int argc, char **args);

void read_line(char *buf, u_int n);
void redisplay_line(char *buf, int len, int cursor_pos);

int try_spawn_command(char *cmd, char **args);
int chdir(const char *path);
int lsh_num_builtins(void);

int execute_backtick_command(char *cmd, char *output, int max_len);
void expand_backticks(char *line);

int split_semicolon(char *line, char *commands[], int *n);
int run_multiple_commands(char *line);

char *builtin_str[] = {"cd", "exit", "pwd", "declare", "unset", "history"};

int (*builtin_func[])(int argc, char **) = {
    &sh_cd, &sh_exit, &sh_pwd, &sh_declare, &sh_unset, &sh_history};

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

    history_fd = open("/.mos_history", O_RDWR | O_CREAT);
    // debugf("sh: history_fd = %d\n", history_fd);
    load_history(); // 启动时加载历史记录
    // debugf("sh: argc %d, interactive %d, echocmds %d\n", argc, interactive,
    // echocmds);

    if (argc > 1) {
        usage();
    }
    if (argc == 1) {
        close(0);
        // debugf("sh: opening script file %s\n", argv[0]);
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

        // 只有非空且非重复的命令才存储
        if (strlen(buf) > 0 &&
            (history_count == 0 ||
             strcmp(buf, history_lines[history_count - 1]) != 0)) {
            store_history(buf);
        }

        trim(buf);
        expand_vars(buf);      // 先展开变量
        expand_backticks(buf); // 再展开反引号
        remove_comments(buf);

        if (buf[0] == '\0' || buf[0] == '#') {
            continue;
        }

        if (echocmds) {
            printf("Executing: %s\n", buf);
        }

        run_multiple_commands(buf);
    }

    return 0;
}

void read_line(char *buf, u_int n) {
    int r;
    int pos = 0;
    int len = 0;

    memset(buf, 0, n);
    history_current = -1;                            // 重置历史浏览位置
    memset(current_input, 0, sizeof(current_input)); // 清空当前输入缓存

    while (1) {
        char c;
        if ((r = read(0, &c, 1)) != 1) {
            if (r < 0) {
                debugf("read error: %d\n", r);
            }
            exit(-1);
        }

        if (c == '\r' || c == '\n') {
            buf[len] = '\0';
            return;
        } else if (c == '\b' || c == 0x7f) {
            if (pos > 0) {
                for (int i = pos - 1; i < len - 1; i++) {
                    buf[i] = buf[i + 1];
                }
                pos--;
                len--;
                buf[len] = '\0';
                redisplay_line(buf, len, pos);
            }
        } else if (c == 1) {
            pos = 0;
            printf("\r$ ");
        } else if (c == 5) {
            pos = len;
            printf("\r$ ");
            for (int i = 0; i < len; i++) {
                printf("%c", buf[i]);
            }
        } else if (c == 11) {
            len = pos;
            buf[len] = '\0';
            redisplay_line(buf, len, pos);
        } else if (c == 21) {
            if (pos > 0) {
                for (int i = 0; i < len - pos; i++) {
                    buf[i] = buf[pos + i];
                }
                len = len - pos;
                pos = 0;
                buf[len] = '\0';
                redisplay_line(buf, len, pos);
            }
        } else if (c == 23) {
            if (pos > 0) {
                int old_pos = pos;
                while (pos > 0 &&
                       (buf[pos - 1] == ' ' || buf[pos - 1] == '\t')) {
                    pos--;
                }
                while (pos > 0 && buf[pos - 1] != ' ' && buf[pos - 1] != '\t') {
                    pos--;
                }
                int chars_to_delete = old_pos - pos;
                for (int i = pos; i < len - chars_to_delete; i++) {
                    buf[i] = buf[i + chars_to_delete];
                }
                len -= chars_to_delete;
                buf[len] = '\0';
                redisplay_line(buf, len, pos);
            }
        } else if (c == 27) {
            char seq[3];
            if (read(0, &seq[0], 1) == 1 && seq[0] == '[') {
                if (read(0, &seq[1], 1) == 1) {
                    if (seq[1] == 'A') {
                        // Up arrow - 上一条历史记录
                        if (history_count > 0) {
                            // 如果是第一次按上箭头，保存当前输入
                            if (history_current == -1) {
                                strcpy(current_input, buf);
                                history_current = history_count - 1;
                            } else if (history_current > 0) {
                                history_current--;
                            }

                            // 复制历史记录到缓冲区
                            strcpy(buf, history_lines[history_current]);
                            len = strlen(buf);
                            pos = len;

                            // 重新显示
                            printf("\r$ ");
                            for (int i = 0; i < len; i++) {
                                printf("%c", buf[i]);
                            }
                            printf("\033[K"); // 清除行尾
                        }
                    } else if (seq[1] == 'B') {
                        // Down arrow - 下一条历史记录
                        if (history_current != -1) {
                            if (history_current < history_count - 1) {
                                history_current++;
                                strcpy(buf, history_lines[history_current]);
                            } else {
                                // 回到当前输入
                                history_current = -1;
                                strcpy(buf, current_input);
                            }

                            len = strlen(buf);
                            pos = len;

                            // 重新显示
                            printf("\r$ ");
                            for (int i = 0; i < len; i++) {
                                printf("%c", buf[i]);
                            }
                            printf("\033[K"); // 清除行尾
                        }
                    } else if (seq[1] == 'C') {
                        if (pos < len) {
                            pos++;
                            printf("\033[C");
                        }
                    } else if (seq[1] == 'D') {
                        if (pos > 0) {
                            pos--;
                            printf("\033[D");
                        }
                    }
                }
            }
        } else if (c >= 32 && c <= 126) {
            if (len < n - 1) {
                for (int i = len; i > pos; i--) {
                    buf[i] = buf[i - 1];
                }
                buf[pos] = c;
                pos++;
                len++;
                buf[len] = '\0';
                redisplay_line(buf, len, pos);
            }
        }
    }
}

void redisplay_line(char *buf, int len, int cursor_pos) {
    printf("\r$ ");
    for (int i = 0; i < len; i++) {
        printf("%c", buf[i]);
    }
    printf("\033[K");
    if (cursor_pos < len) {
        printf("\r$ ");
        for (int i = 0; i < cursor_pos; i++) {
            printf("%c", buf[i]);
        }
    }
}

// 加载历史记录从文件到内存
void load_history(void) {
    if (history_fd < 0) {
        return;
    }

    // 重置文件指针到开头
    seek(history_fd, 0);

    char buffer[4096]; // 一次读取更多数据
    int bytes_read = read(history_fd, buffer, sizeof(buffer) - 1);

    if (bytes_read <= 0) {
        return; // 文件为空或读取失败
    }

    buffer[bytes_read] = '\0'; // 确保字符串结束

    history_count = 0;
    char *line_start = buffer;
    char *line_end;

    while (history_count < HISTFILESIZE && line_start < buffer + bytes_read) {
        // 查找换行符
        line_end = strchr(line_start, '\n');

        if (line_end) {
            *line_end = '\0'; // 替换换行符为字符串结束符
        } else {
            // 最后一行没有换行符
            line_end = buffer + bytes_read;
        }

        // 如果行不为空，添加到历史记录
        if (strlen(line_start) > 0) {
            strncpy(history_lines[history_count], line_start,
                    sizeof(history_lines[history_count]) - 1);
            history_lines[history_count]
                         [sizeof(history_lines[history_count]) - 1] = '\0';
            history_count++;
        }

        // 移动到下一行
        if (line_end < buffer + bytes_read) {
            line_start = line_end + 1;
        } else {
            break;
        }
    }
}

// 保存历史记录到文件
void save_history_to_file(void) {
    if (history_fd < 0) {
        return;
    }

    // 重新打开文件来截断并重写
    close(history_fd);
    history_fd = open("/.mos_history", O_WRONLY | O_CREAT | O_TRUNC);
    if (history_fd < 0) {
        return;
    }

    for (int i = 0; i < history_count; i++) {
        write(history_fd, history_lines[i], strlen(history_lines[i]));
        write(history_fd, "\n", 1);
    }
}

void store_history(char *buf) {
    if (strlen(buf) == 0) {
        return;
    }

    // 去掉末尾的换行符
    int len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
        len--;
    }

    if (len == 0) {
        return;
    }

    // 检查是否与最后一条记录相同
    if (history_count > 0 &&
        strcmp(buf, history_lines[history_count - 1]) == 0) {
        return;
    }

    // 如果历史记录已满，移除最旧的记录
    if (history_count >= HISTFILESIZE) {
        for (int i = 0; i < HISTFILESIZE - 1; i++) {
            strcpy(history_lines[i], history_lines[i + 1]);
        }
        history_count = HISTFILESIZE - 1;
    }

    // 添加新记录
    strcpy(history_lines[history_count], buf);
    history_count++;

    // 保存到文件
    save_history_to_file();
}

int sh_cd(int argc, char **args) {
    static char cur_path[MAXPATHLEN]; // 使用静态分配
    static char new_path[MAXPATHLEN];

    if (args[1] == NULL) {
        chdir("/");
        return 0;
    }

    if (argc > 2) {
        printf("Too many args for cd command\n");
        return 1;
    }

    syscall_get_dir(cur_path);
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

    // debugf("declare called with %d args\n", argc);
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
        // debugf("declare: found %d variables\n", count);
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
            // debugf("the name is %s, value is %s, exported: %d, readonly:
            // %d\n", args[i], eq + 1, exported, readonly);
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

int sh_history(int argc, char **args) {
    if (argc > 1) {
        printf("history: expected 0 arguments; got %d\n", argc - 1);
        return 2;
    }

    // 显示所有历史记录，每行前加序号
    for (int i = 0; i < history_count; i++) {
        printf("%4d  %s\n", i + 1, history_lines[i]);
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

int is_builtin(char *cmd) {
    for (int i = 0; i < lsh_num_builtins(); i++) {
        if (strcmp(cmd, builtin_str[i]) == 0) {
            // debugf("is_builtin: %s found at index %d\n", cmd, i);
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

    // debugf("Running builtin command: %s with %d args\n", args[0], argc);

    return builtin_func[builtin_index](argc, args);
}

int run_with_logic(char *line) {
    char *seg[MAX_CMDS];
    char ops[MAX_CMDS];
    int n = 0;
    int last_status = 0;

    split_logical(line, seg, ops, &n);

    for (int i = 0; i < n; i++) {
        // debugf("Running command %d: %s\n", i, seg[i]);

        last_status = run_single_command(seg[i]);

        // debugf("Command %d returned status: %d\n", i, last_status);

        // 如果是 &&，上一条失败就停止
        if (i < n - 1 && ops[i] == '&' && last_status != 0) {
            break;
        }

        // 如果是 ||，上一条成功就停止
        if (i < n - 1 && ops[i] == '|' && last_status == 0) {
            break;
        }
    }
    return last_status; // 返回最后执行命令的状态
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

void parse_args(char *line, char **args, char **infile, char **outfile,
                int *append_mode) {
    int i = 0, j = 0;
    char *token = strtok(line, WHITESPACE);
    *append_mode = 0; // 默认不是追加模式

    while (token && j < MAX_ARGS - 1) {
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, WHITESPACE);
            *infile = token;
        } else if (strcmp(token, ">>") == 0) {
            token = strtok(NULL, WHITESPACE);
            *outfile = token;
            *append_mode = 1; // 设置追加模式
        } else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, WHITESPACE);
            *outfile = token;
            *append_mode = 0; // 确保是覆盖模式
        } else {
            args[j++] = token;
        }
        token = strtok(NULL, WHITESPACE);
    }
    args[j] = NULL;
}

int run_pipeline(char *cmds[], int index, int ncmds, int input_fd) {
    if (ncmds == 1) {
        // 只有一个命令，直接执行
        char *args[MAX_ARGS];
        char *infile = NULL, *outfile = NULL;
        int append_mode = 0;
        parse_args(cmds[0], args, &infile, &outfile, &append_mode);

        if (infile) {
            input_fd = open(infile, O_RDONLY);
            if (input_fd < 0) {
                printf("Error opening input file: %s\n", infile);
                return -1;
            }
        }

        int out_fd = -1;
        if (outfile) {
            if (append_mode) {
                // 追加模式：打开文件用于追加，如果不存在则创建
                out_fd = open(outfile, O_WRONLY | O_CREAT);
                if (out_fd >= 0) {
                    // 移动到文件末尾
                    struct Stat stat;
                    if (fstat(out_fd, &stat) >= 0) {
                        seek(out_fd, stat.st_size);
                    }
                }
            } else {
                // 覆盖模式：截断文件
                out_fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC);
            }

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

        // debugf("Running command: %s\n", args[0]);

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
            // debugf("Executing command: %s\n", args[0]);
            int cmd = try_spawn_command(args[0], args);
            // debugf("Command %s returned %d\n", args[0], cmd);
            if (cmd < 0) {
                printf("Command not found: %s\n", args[0]);
                exit(1);
            } else {
                int ret = wait(cmd);
                // debugf("Command %s exited with status %d\n", args[0], ret);
                exit(ret);
            }
        } else {
            if (input_fd != 0)
                close(input_fd);
            if (out_fd != -1)
                close(out_fd);
            return wait(child);
        }
    }

    // 多个命令的管道：同时启动所有进程
    int pipes[MAX_CMDS - 1][2]; // 创建所有需要的管道
    int children[MAX_CMDS];     // 存储所有子进程PID

    // 创建所有管道
    for (int i = 0; i < ncmds - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            printf("Pipe creation failed\n");
            return -1;
        }
    }

    // 启动所有子进程
    for (int i = 0; i < ncmds; i++) {
        char *args[MAX_ARGS];
        char *infile = NULL, *outfile = NULL;
        int append_mode = 0;
        parse_args(cmds[i], args, &infile, &outfile, &append_mode);

        children[i] = fork();
        if (children[i] < 0) {
            printf("Fork failed\n");
            return -1;
        } else if (children[i] == 0) {
            // 子进程：设置输入输出重定向

            // 设置输入
            if (i == 0) {
                // 第一个命令
                if (infile) {
                    int fd = open(infile, O_RDONLY);
                    if (fd >= 0) {
                        dup(fd, 0);
                        close(fd);
                    }
                } else if (input_fd != 0) {
                    dup(input_fd, 0);
                    close(input_fd);
                }
            } else {
                // 中间命令：从前一个管道读取
                dup(pipes[i - 1][0], 0);
            }

            // 设置输出
            if (i == ncmds - 1) {
                // 最后一个命令
                if (outfile) {
                    int fd;
                    if (append_mode) {
                        // 追加模式
                        fd = open(outfile, O_WRONLY | O_CREAT);
                        if (fd >= 0) {
                            struct Stat stat;
                            if (fstat(fd, &stat) >= 0) {
                                seek(fd, stat.st_size);
                            }
                        }
                    } else {
                        // 覆盖模式
                        fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC);
                    }

                    if (fd >= 0) {
                        dup(fd, 1);
                        close(fd);
                    }
                }
            } else {
                // 非最后一个命令：写入到下一个管道
                dup(pipes[i][1], 1);
            }

            // 关闭所有管道描述符
            for (int j = 0; j < ncmds - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // 执行命令
            if (is_builtin(args[0]) != -1) {
                run_builtin(args);
                exit(0);
            } else {
                int cmd = try_spawn_command(args[0], args);
                if (cmd < 0) {
                    printf("Command not found: %s\n", args[0]);
                    exit(1);
                } else {
                    int ret = wait(cmd);
                    exit(ret);
                }
            }
        }
    }

    // 父进程：关闭所有管道描述符
    for (int i = 0; i < ncmds - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    if (input_fd != 0) {
        close(input_fd);
    }

    // 等待所有子进程完成
    int last_status = 0;
    for (int i = 0; i < ncmds; i++) {
        last_status = wait(children[i]);
    }

    return last_status;
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
            while (*p && !strchr(WHITESPACE SYMBOLS "/", *p) &&
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

int try_spawn_command(char *cmd, char **args) {
    int result;
    char cmd_with_b[256];

    // 首先尝试不带.b后缀的命令
    // debugf("Trying to spawn command: %s\n", cmd);
    result = spawn(cmd, args);
    // debugf("Spawn result for %s: %d\n", cmd, result);
    if (result >= 0) {
        return result;
    }

    // 如果失败，尝试带.b后缀的命令
    if (strlen(cmd) + 2 < sizeof(cmd_with_b)) {
        strcpy(cmd_with_b, cmd);
        strcat(cmd_with_b, ".b");

        // 更新args[0]为带.b后缀的版本
        args[0] = cmd_with_b;
        // debugf("Trying to spawn command with .b: %s\n", cmd_with_b);
        result = spawn(cmd_with_b, args);
        if (result >= 0) {
            return result;
        }
    }

    return -1; // 两种都尝试失败
}

int chdir(const char *path) {
    // debugf("Changed directory to %s\n", path);
    if (syscall_ch_dir(path) < 0) {
        debugf("chdir %s failed\n", path);
        return -1;
    }
    return 0;
}

int lsh_num_builtins(void) { return sizeof(builtin_str) / sizeof(char *); }

int execute_backtick_command(char *cmd, char *output, int max_len) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        return -1;
    }

    int child = fork();
    if (child < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    } else if (child == 0) {
        // 子进程：执行命令并将输出重定向到管道
        close(pipefd[0]);
        dup(pipefd[1], 1); // 将stdout重定向到管道写端
        close(pipefd[1]);

        // 执行命令
        run_single_command(cmd);
        exit(0);
    } else {
        // 父进程：从管道读取输出
        close(pipefd[1]);

        int total_read = 0;
        char buffer[1024];
        int bytes_read;

        // 读取所有输出
        while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            int copy_len = (bytes_read < max_len - total_read - 1)
                               ? bytes_read
                               : (max_len - total_read - 1);
            if (copy_len > 0) {
                memcpy(output + total_read, buffer, copy_len);
                total_read += copy_len;
            }
            if (total_read >= max_len - 1)
                break;
        }

        close(pipefd[0]);
        wait(child);

        output[total_read] = '\0';

        // 移除输出末尾的换行符
        while (total_read > 0 && (output[total_read - 1] == '\n' ||
                                  output[total_read - 1] == '\r')) {
            output[--total_read] = '\0';
        }

        return total_read;
    }
}

void expand_backticks(char *line) {
    char result[1024] = "";
    char *dest = result;
    char *src = line;

    while (*src) {
        if (*src == '`') {
            src++; // 跳过开始的反引号
            char *cmd_start = src;

            // 找到结束的反引号
            while (*src && *src != '`') {
                src++;
            }

            if (*src == '`') {
                // 提取命令
                int cmd_len = src - cmd_start;
                char cmd[1024];
                strncpy(cmd, cmd_start, cmd_len);
                cmd[cmd_len] = '\0';

                // 执行命令并获取输出
                char cmd_output[1024];
                if (execute_backtick_command(cmd, cmd_output,
                                             sizeof(cmd_output)) >= 0) {
                    // 将输出添加到结果中，用空格替换换行符
                    for (char *p = cmd_output; *p; p++) {
                        if (*p == '\n' || *p == '\r') {
                            if (dest - result < sizeof(result) - 1) {
                                *dest++ = ' ';
                            }
                        } else {
                            if (dest - result < sizeof(result) - 1) {
                                *dest++ = *p;
                            }
                        }
                    }
                }
                src++; // 跳过结束的反引号
            } else {
                // 没有找到结束的反引号，保留原始字符
                if (dest - result < sizeof(result) - 1) {
                    *dest++ = '`';
                }
                src = cmd_start; // 回到命令开始位置
            }
        } else {
            // 普通字符，直接复制
            if (dest - result < sizeof(result) - 1) {
                *dest++ = *src;
            }
            src++;
        }
    }

    *dest = '\0';
    strcpy(line, result);
}

int split_semicolon(char *line, char *commands[], int *n) {
    *n = 0;
    char *p = line;
    char *start = line;

    while (*p && *n < MAX_CMDS - 1) {
        if (*p == ';') {
            *p = '\0'; // 分割字符串
            commands[(*n)++] = start;
            start = p + 1;
        }
        p++;
    }

    // 添加最后一个命令（如果有的话）
    if (start < p) {
        commands[(*n)++] = start;
    }

    return *n;
}

int run_multiple_commands(char *line) {
    char *commands[MAX_CMDS];
    int n = 0;
    int last_status = 0;

    split_semicolon(line, commands, &n);

    for (int i = 0; i < n; i++) {
        // 去除每个命令前后的空格
        char *cmd = commands[i];
        while (*cmd && strchr(WHITESPACE, *cmd)) {
            cmd++;
        }

        if (*cmd) { // 如果命令非空
            last_status = run_with_logic(cmd);
        }
    }

    return last_status;
}