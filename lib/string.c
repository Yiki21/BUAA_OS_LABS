#include <string.h>
#include <types.h>

char *strcat(char *dest, const char *src) {
    char *p = dest;

    // 找到 dest 的末尾
    while (*p) {
        p++;
    }

    // 复制 src 到末尾
    while (*src) {
        *p++ = *src++;
    }

    *p = '\0'; // 添加字符串结束符

    return dest;
}

char *strncat(char *dest, const char *src, size_t n) {
    char *p = dest;

    // 找到 dest 的末尾
    while (*p) {
        p++;
    }

    // 复制最多 n 个字符
    while (n-- && *src) {
        *p++ = *src++;
    }

    *p = '\0'; // 添加字符串结束符

    return dest;
}

char *strtok(char *str, const char *delim) {
    static char *next = NULL;
    if (str != NULL) {
        next = str;
    }
    if (next == NULL) {
        return NULL;
    }

    // 跳过前缀的分隔符
    char *start = next;
    while (*start && strchr(delim, *start)) {
        start++;
    }

    if (*start == '\0') {
        next = NULL;
        return NULL;
    }

    // 找下一个分隔符
    char *token_end = start;
    while (*token_end && !strchr(delim, *token_end)) {
        token_end++;
    }

    if (*token_end) {
        *token_end = '\0';
        next = token_end + 1;
    } else {
        next = NULL;
    }

    return start;
}

char *strtok_r(char *str, const char *delim, char **saveptr) {
    char *start;

    if (str != NULL) {
        *saveptr = str;
    }

    if (*saveptr == NULL) {
        return NULL;
    }

    // 跳过前缀的分隔符
    start = *saveptr;
    while (*start && strchr(delim, *start)) {
        start++;
    }

    if (*start == '\0') {
        *saveptr = NULL;
        return NULL;
    }

    // 找下一个分隔符
    char *token_end = start;
    while (*token_end && !strchr(delim, *token_end)) {
        token_end++;
    }

    if (*token_end) {
        *token_end = '\0';
        *saveptr = token_end + 1;
    } else {
        *saveptr = NULL;
    }

    return start;
}

void *memcpy(void *dst, const void *src, size_t n) {
    void *dstaddr = dst;
    void *max = dst + n;

    if (((u_long)src & 3) != ((u_long)dst & 3)) {
        while (dst < max) {
            *(char *)dst++ = *(char *)src++;
        }
        return dstaddr;
    }

    while (((u_long)dst & 3) && dst < max) {
        *(char *)dst++ = *(char *)src++;
    }

    // copy machine words while possible
    while (dst + 4 <= max) {
        *(uint32_t *)dst = *(uint32_t *)src;
        dst += 4;
        src += 4;
    }

    // finish the remaining 0-3 bytes
    while (dst < max) {
        *(char *)dst++ = *(char *)src++;
    }
    return dstaddr;
}

void *memset(void *dst, int c, size_t n) {
    void *dstaddr = dst;
    void *max = dst + n;
    u_char byte = c & 0xff;
    uint32_t word = byte | byte << 8 | byte << 16 | byte << 24;

    while (((u_long)dst & 3) && dst < max) {
        *(u_char *)dst++ = byte;
    }

    // fill machine words while possible
    while (dst + 4 <= max) {
        *(uint32_t *)dst = word;
        dst += 4;
    }

    // finish the remaining 0-3 bytes
    while (dst < max) {
        *(u_char *)dst++ = byte;
    }
    return dstaddr;
}

size_t strlen(const char *s) {
    int n;

    for (n = 0; *s; s++) {
        n++;
    }

    return n;
}

char *strcpy(char *dst, const char *src) {
    char *ret = dst;

    while ((*dst++ = *src++) != 0) {
    }

    return ret;
}

const char *strchr(const char *s, int c) {
    for (; *s; s++) {
        if (*s == c) {
            return s;
        }
    }
    return 0;
}

int strcmp(const char *p, const char *q) {
    while (*p && *p == *q) {
        p++, q++;
    }

    if ((u_int)*p < (u_int)*q) {
        return -1;
    }

    if ((u_int)*p > (u_int)*q) {
        return 1;
    }

    return 0;
}

/* Overview:
 *   Resolve a relative or absolute path based on current working directory.
 *
 * Pre-Condition:
 *   'origin_path' is the path to resolve.
 *   'cur_path' is the current working directory.
 *   'resolved_path' is the output buffer.
 *   'max_len' is the size of the output buffer.
 *
 * Post-Condition:
 *   Return 0 on success, resolved path is stored in 'resolved_path'.
 *   Return -1 on error (invalid parameters, buffer too small, etc.).
 */
int resolve_path(const char *origin_path, const char *cur_path,
                 char *resolved_path, size_t max_len) {
    char temp[MAXPATHLEN];
    char work_copy[MAXPATHLEN];

    // 检查参数
    if (origin_path == NULL || resolved_path == NULL || max_len == 0) {
        return -1;
    }

    if (cur_path == NULL) {
        cur_path = "/";
    }

    // 处理绝对路径
    if (origin_path[0] == '/') {
        if (strlen(origin_path) >= max_len) {
            return -1;
        }
        strcpy(resolved_path, origin_path);
        return 0;
    }

    // 处理相对路径
    // 构建完整路径
    if (strlen(cur_path) + strlen(origin_path) + 2 >= MAXPATHLEN) {
        return -1;
    }

    strcpy(temp, cur_path);

    // 确保当前路径以 / 结尾（除非是根目录）
    size_t cur_len = strlen(temp);
    if (cur_len > 1 && temp[cur_len - 1] != '/') {
        strcat(temp, "/");
    }
    strcat(temp, origin_path);

    // 复制到工作缓冲区进行规范化
    strcpy(work_copy, temp);

    // 规范化路径：处理 . 和 ..
    char *components[256]; // 路径组件数组
    int component_count = 0;

    // 分割路径
    char *token = strtok(work_copy, "/");
    while (token != NULL && component_count < 256) {
        if (strcmp(token, ".") == 0) {
            // 当前目录，忽略
        } else if (strcmp(token, "..") == 0) {
            // 上级目录，删除一个组件
            if (component_count > 0) {
                component_count--;
            }
        } else if (strlen(token) > 0) {
            // 普通目录名
            components[component_count++] = token;
        }
        token = strtok(NULL, "/");
    }

    // 重新构建路径
    resolved_path[0] = '\0';
    strcat(resolved_path, "/");

    for (int i = 0; i < component_count; i++) {
        if (strlen(resolved_path) + strlen(components[i]) + 2 >= max_len) {
            return -1; // 缓冲区太小
        }

        if (i > 0) {
            strcat(resolved_path, "/");
        }
        strcat(resolved_path, components[i]);
    }

    return 0;
}

char *strstr(const char *haystack, const char *needle) {
    // 如果 needle 为空字符串，返回 haystack
    if (*needle == '\0') {
        return (char *)haystack;
    }

    // 遍历 haystack 中的每个位置
    while (*haystack) {
        const char *h = haystack;
        const char *n = needle;

        // 比较从当前位置开始的子串
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }

        // 如果 needle 完全匹配，返回当前位置
        if (*n == '\0') {
            return (char *)haystack;
        }

        haystack++;
    }

    // 没有找到匹配的子串
    return NULL;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;

    // 复制最多 n 个字符
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }

    // 如果复制了少于 n 个字符，用 '\0' 填充剩余部分
    for (; i < n; i++) {
        dest[i] = '\0';
    }

    return dest;
}