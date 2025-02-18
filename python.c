#include "Python.h"
#include <asm/prctl.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/syscall.h>

int
main(int argc, char **argv)
{
    fprintf(stderr, "main() -- should never be called\n");
    return 0;
}

__attribute__ ((format(printf, 1, 2)))
static void fprintf_exit(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

static void perror_exit(char *msg)
{
    perror(msg);
    exit(1);
}

#define SONAME_MAX 256

static void so_path(char *name)
{
    uintptr_t low, high, ptr = (uintptr_t)&main;
    size_t name_len = 0;
    int maps_fd;
    ssize_t r;
    char c;

    maps_fd = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    if (maps_fd < 0)
        perror_exit("open(/proc/self/maps)");

line:
    low = high = 0;

    while (true) {
        r = read(maps_fd, &c, 1);
        if (!r)
            goto not_found;
        if (r != 1)
            goto err;

        if (c == '-')
            break;

        if ('0' <= c && c <= '9')
            low = low * 16 + c - '0';
        else if ('a' <= c && c <= 'f')
            low = low * 16 + c - 'a' + 10;
        else
            goto err;
    }

    while (true) {
        r = read(maps_fd, &c, 1);
        if (!r)
            goto not_found;
        if (r != 1)
            goto err;

        if (c == ' ')
            break;

        if ('0' <= c && c <= '9')
            high = high * 16 + c - '0';
        else if ('a' <= c && c <= 'f')
            high = high * 16 + c - 'a' + 10;
        else
            goto err;
    }

    if (low <= ptr && ptr < high) {
        size_t column = 0;
        bool in_column = false;

        while (true) {
            r = read(maps_fd, &c, 1);
            if (!r)
                break;
            if (r != 1)
                goto err;
            if (c == '\n')
                break;

            bool new_in_col = c != ' ';
            if (new_in_col && !in_column)
                column++;
            in_column = new_in_col;

            if (in_column && column == 5) {
                name[name_len++] = c;
                if (name_len + 1 >= SONAME_MAX)
                    goto err;
            }
        }

        name[name_len] = '\0';
        goto found;
    } else {
        while (true) {
            r = read(maps_fd, &c, 1);
            if (!r)
                goto not_found;
            if (r != 1)
                goto err;
            if (c == '\n')
                goto line;
        }
    }

found:
    return;

err:
not_found:
    fprintf_exit("so_path: error finding path of preload");
}

extern void __init_libc(char **envp, char *pn);
unsigned long saved_fs;

__attribute__((constructor(65535)))
static void realmain(int argc, char **argv, char **env)
{
#ifdef __x86_64__
    if (syscall(SYS_arch_prctl, ARCH_GET_FS, &saved_fs))
        perror_exit("arch_prctl(ARCH_GET_FS)");
#else
# error "Unsupported arch"
#endif

    __init_libc(env, NULL);

    char name[SONAME_MAX];
    so_path(name);

    PyStatus status;
    PyConfig config;
    PyConfig_InitIsolatedConfig(&config);

    status = Py_InitializeFromConfig(&config);
    if (PyStatus_Exception(status)) {
        Py_ExitStatusException(status);
    }
    PyConfig_Clear(&config);

    PyObject *sys_path = PySys_GetObject("path");
    if (!sys_path) {
        PyErr_SetString(PyExc_RuntimeError, "Can't get sys.path");
        goto out;
    }

    PyObject *selfpath = PyUnicode_FromString(name);
    if (!selfpath)
        goto out;

    if (PyList_Insert(sys_path, 0, selfpath))
        goto out;

    PyImport_ImportModule("main");

out:
    if (PyErr_Occurred())
        PyErr_Print();

    Py_Finalize();

#ifdef __x86_64__
    if (syscall(SYS_arch_prctl, ARCH_SET_FS, saved_fs))
        perror_exit("arch_prctl(ARCH_SET_FS)");
#else
# error "Unsupported arch"
#endif
}
