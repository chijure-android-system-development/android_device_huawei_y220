/*
 * dltest: dlopen each library given, each in its own child, and print
 * "OK" or the linker error. A NEEDED check misses unresolved symbols;
 * the GB linker only reports those when it loads the library.
 */
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char **argv)
{
    int i, status;

    for (i = 1; i < argc; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            void *h = dlopen(argv[i], RTLD_NOW);
            const char *err = h ? "OK" : dlerror();
            char *p;

            /* One line per library. */
            for (p = (char *)err; *p; p++)
                if (*p == '\n')
                    *p = ' ';
            printf("%s: %s\n", argv[i], err);
            fflush(stdout);
            _exit(0);
        }
        if (pid < 0 || waitpid(pid, &status, 0) < 0 ||
                !WIFEXITED(status) || WEXITSTATUS(status) != 0)
            printf("%s: CRASH\n", argv[i]);
    }
    return 0;
}
