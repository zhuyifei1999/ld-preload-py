#include "Python.h"
#include <asm/prctl.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <wchar.h>

#define noreturn __attribute__((noreturn))
#define always_inline __attribute__((always_inline))

int
main(int argc, char **argv)
{
    fprintf(stderr, "main() -- should never be called\n");
    return 0;
}

static inline
long _z_syscall(long n, ...)
{
    long a, b, c, d, e, f;
    unsigned long ret;
    va_list ap;

    va_start(ap, n);
    a = va_arg(ap, long);
    b = va_arg(ap, long);
    c = va_arg(ap, long);
    d = va_arg(ap, long);
    e = va_arg(ap, long);
    f = va_arg(ap, long);
    va_end(ap);

#ifdef __x86_64__
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    register long r9 __asm__("r9") = f;
    __asm__ __volatile__ (
        "syscall" :
        "=a"(ret) :
        "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8), "r"(r9) :
        "rcx", "r11", "memory"
    );
#else
# error "Unsupported arch"
#endif
    return ret;
}

static inline always_inline
long _z_sys_arch_prctl(int op, unsigned long addr)
{
    /* Must always_inline due to stack canary */
    unsigned long ret;

#ifdef __x86_64__
    __asm__ __volatile__ (
        "syscall" :
        "=a"(ret) :
        "a"(SYS_arch_prctl), "D"(op), "S"(addr) :
        "rcx", "r11", "memory"
    );
#else
# error "Unsupported arch"
#endif
    return ret;
}

static inline noreturn
void _z_sys_exit(int status)
{
    _z_syscall(SYS_exit_group, status);
    for (;;)
        __asm__ __volatile__ ("ud2");
}
static inline
int _z_sys_open(const char *pathname, int flags)
{
    return _z_syscall(SYS_open, pathname, flags);
}
static inline
int _z_sys_close(int fd)
{
    return _z_syscall(SYS_close, fd);
}
static inline
ssize_t _z_sys_read(int fd, void *buf, size_t count)
{
    return _z_syscall(SYS_read, fd, buf, count);
}
static inline
ssize_t _z_sys_write(int fd, const void *buf, size_t count)
{
    return _z_syscall(SYS_write, fd, buf, count);
}

static inline
size_t _z_strlen(const char *s)
{
    size_t len = 0;
    while (s[len])
        len++;
    return len;
}

static noreturn
void _z_die(const char *message)
{
    _z_sys_write(STDERR_FILENO, message, _z_strlen(message));
    _z_sys_exit(1);
}

#define SONAME_MAX 256

static void so_path(char *name)
{
    uintptr_t low, high, ptr = (uintptr_t)&main;
    size_t name_len = 0;
    int maps_fd;
    ssize_t r;
    char c;

    maps_fd = _z_sys_open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    if (maps_fd < 0)
        _z_die("open(/proc/self/maps) failed\n");

line:
    low = high = 0;

    while (true) {
        r = _z_sys_read(maps_fd, &c, 1);
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
        r = _z_sys_read(maps_fd, &c, 1);
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
            r = _z_sys_read(maps_fd, &c, 1);
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
            r = _z_sys_read(maps_fd, &c, 1);
            if (!r)
                goto not_found;
            if (r != 1)
                goto err;
            if (c == '\n')
                goto line;
        }
    }

found:
    _z_sys_close(maps_fd);
    return;

err:
not_found:
    _z_die("so_path failed\n");
}

extern void __init_libc(char **envp, char *pn);
static unsigned long saved_fs;

char ***_z_import_environ;
extern char **environ;

__attribute__((constructor(65535)))
static void realmain(void)
{
#ifdef __x86_64__
    if (_z_sys_arch_prctl(ARCH_GET_FS, (unsigned long)&saved_fs))
        _z_die("arch_prctl(ARCH_GET_FS) failed\n");
#else
# error "Unsupported arch"
#endif

    if (!_z_import_environ)
        _z_import_environ = &environ;

    __init_libc(*_z_import_environ, NULL);

    wchar_t name_wide[SONAME_MAX];
    char name[SONAME_MAX];
    const char *pname = name;
    so_path(name);

    if (mbsrtowcs(name_wide, &pname, SONAME_MAX, NULL) == (size_t)-1)
        _z_die("mbsrtowcs failed\n");

    PyStatus status;
    PyConfig config;
    PyConfig_InitIsolatedConfig(&config);

    status = PyWideStringList_Insert(&config.module_search_paths,
                                     0, name_wide);
    if (PyStatus_Exception(status))
        Py_ExitStatusException(status);
    config.module_search_paths_set = 1;
    config.site_import = 0;

    status = Py_InitializeFromConfig(&config);
    if (PyStatus_Exception(status))
        Py_ExitStatusException(status);
    PyConfig_Clear(&config);

    PyImport_ImportModule("main");

    if (PyErr_Occurred())
        PyErr_Print();

    Py_Finalize();

#ifdef __x86_64__
    if (_z_sys_arch_prctl(ARCH_SET_FS, saved_fs))
        _z_die("arch_prctl(ARCH_SET_FS) failed\n");
#else
# error "Unsupported arch"
#endif
}
