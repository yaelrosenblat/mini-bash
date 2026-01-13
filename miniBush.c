#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define MAX_ARGS 64
#define BUF 128

// dynamic buffer for efficiency
// my string functions 

int my_strlen(const char *s) {
    int i = 0;
    while (s[i] != '\0') i++;
    return i;
}

int my_strcmp(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i])
            return a[i] - b[i];
        i++;
    }
    return a[i] - b[i];
}

void my_strcpy(char *dst, const char *src) {
    while (*src) {
        *dst++ = *src++;
    }
    *dst = '\0';
}

// write string base on write system call

void write_str(int fd, const char *s) {
    write(fd, s, my_strlen(s));
}

// maneging dynamic buffer

ssize_t read_line(char **buffer, size_t *size) {
    if (*buffer == NULL) {
        *size = BUF;
        *buffer = malloc(*size);
    }

    size_t pos = 0;
    char c;

    while (1) {
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0)
            return -1;

        if (c == '\n')
            break;

        if (pos + 1 >= *size) {
            *size += BUF;
            *buffer = realloc(*buffer, *size);
        }

        (*buffer)[pos++] = c;
    }

    (*buffer)[pos] = '\0';
    return pos;
}

// Parse line in place

int parse_line(char *line, char **args) {
    int count = 0;
    int i = 0;

    while (line[i] != '\0') {
        while (line[i] == ' ' || line[i] == '\t') {
            line[i++] = '\0';
        }

        if (line[i] != '\0') {
            args[count++] = &line[i];
        }

        while (line[i] != '\0' &&
               line[i] != ' ' &&
               line[i] != '\t') {
            i++;
        }
    }

    args[count] = NULL;
    return count;
}

// Internal commands

int handle_internal(char **args) {
    if (my_strcmp(args[0], "exit") == 0) {
        exit(0);
    }

    if (my_strcmp(args[0], "cd") == 0) {
        if (!args[1]) {
            write_str(STDERR_FILENO, "cd: missing argument\n");
        } else if (chdir(args[1]) != 0) {
            write_str(STDERR_FILENO, "cd failed\n");
        }
        return 1;
    }

    return 0;
}

// Executable search

int is_executable(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 &&
            S_ISREG(st.st_mode) &&
            access(path, X_OK) == 0);
}

char *find_executable(char *cmd) {
    char path[512];
    char *home = getenv("HOME");
    int i, j;
// search at home diractory
    if (home) {
        i = 0;
        while (home[i]) {
            path[i] = home[i];
            i++;
        }
        path[i++] = '/';

        j = 0;
        while (cmd[j]) {
            path[i++] = cmd[j++];
        }
        path[i] = '\0';

        if (is_executable(path)) {
            char *res = malloc(i + 1);
            my_strcpy(res, path);
            return res;
        }
    }
//search at bin
    my_strcpy(path, "/bin/");
    i = 5;
    j = 0;
    while (cmd[j]) {
        path[i++] = cmd[j++];
    }
    path[i] = '\0';

    if (is_executable(path)) {
        char *res = malloc(i + 1);
        my_strcpy(res, path);
        return res;
    }

    return NULL;
}

// Main shell loop

int main() {
    char *line = NULL;
    size_t buf_size = 0;
    char *args[MAX_ARGS];

    while (1) {
        write_str(STDOUT_FILENO, "mini-bash$ ");

        if (read_line(&line, &buf_size) < 0)
            break;

        int argc = parse_line(line, args);
        if (argc == 0)
            continue;

        if (handle_internal(args))
            continue;

        char *exec_path = find_executable(args[0]);
        if (!exec_path) {
            write_str(STDERR_FILENO, "Unknown Command\n");
            continue;
        }
// fork
        pid_t pid = fork();

        if (pid == 0) {
            execv(exec_path, args);
            write_str(STDERR_FILENO, "exec failed\n");
            exit(1);
        } else {
            int status;
            waitpid(pid, &status, 0);
        }

        free(exec_path);
    }

    free(line);
    return 0;
}
