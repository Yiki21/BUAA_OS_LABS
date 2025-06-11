#include <args.h>
#include <lib.h>

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()"

/* Overview:
 *   Parse the next token from the string at s.
 *
 * Post-Condition:
 *   Set '*p1' to the beginning of the token and '*p2' to just past the token.
 *   Return:
 *     - 0 if the end of string is reached.
 *     - '<' for < (stdin redirection).
 *     - '>' for > (stdout redirection).
 *     - '|' for | (pipe).
 *     - 'w' for a word (command, argument, or file name).
 *
 *   The buffer is modified to turn the spaces after words into zero bytes
 * ('\0'), so that the returned token is a null-terminated string.
 */
int _gettoken(char *s, char **p1, char **p2) {
    *p1 = 0;
    *p2 = 0;
    if (s == 0) {
        return 0;
    }

    while (strchr(WHITESPACE, *s)) {
        *s++ = 0;
    }
    if (*s == 0) {
        return 0;
    }

    if (strchr(SYMBOLS, *s)) {
        int t = *s;
        *p1 = s;
        *s++ = 0;
        *p2 = s;
        return t;
    }

    *p1 = s;
    while (*s && !strchr(WHITESPACE SYMBOLS, *s)) {
        s++;
    }
    *p2 = s;
    return 'w';
}

int gettoken(char *s, char **p1) {
    static int c, nc;
    static char *np1, *np2;

    if (s) {
        nc = _gettoken(s, &np1, &np2);
        return 0;
    }
    c = nc;
    *p1 = np1;
    nc = _gettoken(np2, &np1, &np2);
    return c;
}

#define MAXPATHLEN 1024
char cur_path[MAXPATHLEN] =
    "/"; // Current working directory, initialized to root

char *resolve_path(const char *origin_path) {
    static char resolved_path[MAXPATHLEN];
    char temp[MAXPATHLEN];

    if (origin_path[0] == '/') {
        // Absolute path, return as is
        strcpy(resolved_path, origin_path);
        return resolved_path;
    }

    // Relative path, prepend current working directory
    strcpy(temp, cur_path);
    if (temp[strlen(temp) - 1] != '/') {
        strcat(temp, "/");
    }
    strcat(temp, origin_path);
    // Normalize the path

    char *tokens[256];
    int token_count = 0;
    char *token = strtok(temp, "/");

    while (token && token_count < 256) {
        if (strcmp(token, ".") == 0) {
            // 忽略当前目录符号，不做任何操作
        } else if (strcmp(token, "..") == 0) {
            if (token_count > 0) {
                token_count--;
            }
        } else {
            tokens[token_count++] = token;
        }
        token = strtok(NULL, "/");
    }

    // Reconstruct the path
    strcpy(resolved_path, "/");
    for (int i = 0; i < token_count; i++) {
        if (i > 0) {
            strcat(resolved_path, "/");
        }
        strcat(resolved_path, tokens[i]);
    }

    return resolved_path;
}

int builtin_cd(char **argv) {
	if (argv[1] == NULL) {
		strcpy(cur_path, "/");
		return 0;
	} else if (argv[2] != NULL) {
		printf("Too many args for cd command\n");
		return 1;
	} else {
		char *new_path = resolve_path(argv[1]);
		struct Stat st;
		if (stat(new_path, &st) < 0) {
			printf("cd: The directory '%s' does not exist\n", argv[1]);
			return 1;
		}
		// Check if it's actually a directory
        if (!st.st_isdir) {
			printf("filePath: '%s' is not a directory\n", new_path);
			printf("cd: '%s' is not a directory\n", argv[1]);
			return 1;
        }
		strcpy(cur_path, new_path);
		return 0;
	}
}

int builtin_pwd(char **argv) {
    // Count arguments (excluding command name)
    int arg_count = 0;
    while (argv[arg_count + 1] != NULL) {
        arg_count++;
    }
    
    if (arg_count > 0) {
        printf("pwd: expected 0 arguments; got %d\n", arg_count);
        return 2;
    }
    
    printf("%s\n", cur_path);
    return 0;
}

static int last_exit_status = 0;

int is_builtin(char **argv) {
    if (strcmp(argv[0], "cd") == 0) {
        last_exit_status = builtin_cd(argv);
        return 1;
    }
    if (strcmp(argv[0], "pwd") == 0) {
        last_exit_status = builtin_pwd(argv);
        return 1;
    }
    return 0;
}

#define MAXARGS 128

int parsecmd(char **argv, int *rightpipe) {
    int argc = 0;
    while (1) {
        char *t;
        int fd, r;
        int c = gettoken(0, &t);
        switch (c) {
        case 0:
            return argc;
        case 'w':
            if (argc >= MAXARGS) {
                debugf("too many arguments\n");
                exit();
            }
            argv[argc++] = t;
            break;
        case '<':
            if (gettoken(0, &t) != 'w') {
                debugf("syntax error: < not followed by word\n");
                exit();
            }
            // Open 't' for reading, dup it onto fd 0, and then close the
            // original fd. If the 'open' function encounters an error, utilize
            // 'debugf' to print relevant messages, and subsequently terminate
            // the process using 'exit'.
            /* Exercise 6.5: Your code here. (1/3) */
            if ((fd = open(t, O_RDONLY)) < 0) {
                debugf("open %s: %d\n", t, fd);
                exit();
            }
            dup(fd, 0);
            close(fd);

            break;
        case '>':
            if (gettoken(0, &t) != 'w') {
                debugf("syntax error: > not followed by word\n");
                exit();
            }
            // Open 't' for writing, create it if not exist and trunc it if
            // exist, dup it onto fd 1, and then close the original fd. If the
            // 'open' function encounters an error, utilize 'debugf' to print
            // relevant messages, and subsequently terminate the process using
            // 'exit'.
            /* Exercise 6.5: Your code here. (2/3) */
            if ((fd = open(t, O_WRONLY | O_CREAT | O_TRUNC)) < 0) {
                debugf("open %s: %d\n", t, fd);
                exit();
            }
            dup(fd, 1);
            close(fd);

            break;
        case '|':;
            /*
             * First, allocate a pipe.
             * Then fork, set '*rightpipe' to the returned child envid or zero.
             * The child runs the right side of the pipe:
             * - dup the read end of the pipe onto 0
             * - close the read end of the pipe
             * - close the write end of the pipe
             * - and 'return parsecmd(argv, rightpipe)' again, to parse the rest
             * of the command line. The parent runs the left side of the pipe:
             * - dup the write end of the pipe onto 1
             * - close the write end of the pipe
             * - close the read end of the pipe
             * - and 'return argc', to execute the left of the pipeline.
             */
            int p[2];
            /* Exercise 6.5: Your code here. (3/3) */
            if ((r = pipe(p)) < 0) {
                debugf("pipe: %d\n", r);
                exit();
            }
            *rightpipe = fork();
            if (*rightpipe < 0) {
                exit();
            }
            if (*rightpipe == 0) {
                // Child process
                dup(p[0], 0); // dup read end to fd 0
                close(p[0]);  // close read end
                close(p[1]);  // close write end
                return parsecmd(argv, rightpipe);
            } else {
                // Parent process
                dup(p[1], 1); // dup write end to fd 1
                close(p[1]);  // close write end
                close(p[0]);  // close read end
                return argc;  // return argc to execute the left side of the
                              // pipeline
            }

            break;
        }
    }

    return argc;
}

void runcmd(char **argv, int argc, int rightpipe) {
    if (argc == 0) {
        return;
    }
    //printf("%s, %s\n", argv[0], argv[1] ? argv[1] : "(null)");
    int child = spawn(argv[0], argv);
    close_all();
    if (child >= 0) {
        wait(child);
    } else {
        debugf("spawn %s: %d\n", argv[0], child);
        last_exit_status = -1;
    }
    if (rightpipe) {
        wait(rightpipe);
    }
    exit();
}

void readline(char *buf, u_int n) {
    int r;
    for (int i = 0; i < n; i++) {
        if ((r = read(0, buf + i, 1)) != 1) {
            if (r < 0) {
                debugf("read error: %d\n", r);
            }
            exit();
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

char buf[1024];

void usage(void) {
    printf("usage: sh [-ix] [script-file]\n");
    exit();
}

int main(int argc, char **argv) {
    int r;
    int interactive = iscons(0);
    int echocmds = 0;
    printf("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
    printf("::                                                         ::\n");
    printf("::                     MOS Shell 2024                      ::\n");
    printf("::                                                         ::\n");
    printf(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
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
    for (;;) {
        if (interactive) {
            printf("\n$ ");
        }
        readline(buf, sizeof buf);

        if (buf[0] == '#') {
            continue;
        }
        if (echocmds) {
            printf("# %s\n", buf);
        }

		gettoken(buf, 0);
		char *argv[MAXARGS];
		int rightpipe = 0;
		int argc = parsecmd(argv, &rightpipe);
		if (argc > 0) {
			argv[argc] = 0;
			if (is_builtin(argv)) {
				if (rightpipe) {
					wait(rightpipe);
				}
				continue;
			}
		}

        if ((r = fork()) < 0) {
            user_panic("fork: %d", r);
        }
        if (r == 0) {
            runcmd(argv, argc, rightpipe);  // 传递已解析的参数
            exit();
        } else {
            wait(r);
        }
    }
    return 0;
}
