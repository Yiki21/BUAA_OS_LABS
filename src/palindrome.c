#include <stdio.h>

int is_palindrome(int num) {
    int digit[5], j = 0, i = 0;
    while (num) {
        digit[j++] = num % 10;
        num /= 10;
    }
    j--;
    for (; i <= j; i++, j--) {
        if (digit[i] != digit[j])
            return 0;
    }
    return 1;
}

int main() {
    int n;
    scanf("%d", &n);
    if (is_palindrome(n)) {
        printf("Y\n");
    } else {
        printf("N\n");
    }
}
