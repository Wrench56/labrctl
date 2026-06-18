#include "cforge.h"

#define CC_TAG "[" CF_YELLOW "CC" CF_RESET "] "
#define AR_TAG "[" CF_GREEN "AR" CF_RESET "] "
#define LD_TAG "[" CF_CYAN "LD" CF_RESET "] "
#define KO_TAG "[" CF_MAGENTA "KO" CF_RESET "] "

#define BUILD_DIR "build"

#define KMOD_NAME "labrctl"
#define KMOD_KO KMOD_NAME ".ko"
#define KMOD_DIR "src/kmod"
#define KMOD_BUILD_DIR BUILD_DIR "/kmod"

#define LIB_DIR "src/lib"
#define LIB_BUILD_DIR BUILD_DIR "/lib"
#define LIB BUILD_DIR "/liblabrctl.a"

#define BPF_CLANG "clang"
#define XPD_DIR "src/xpd"
#define XPD_OBJ BUILD_DIR "/xpdfwd.o"
#define XPD_SECTION "xdp_fwd"

CF_CONFIG(bpf)
{
    CF_SET_ENV(bpfcc, BPF_CLANG);
    CF_SET_ENV(
        bpfflags,
        "-O2 "
        "-g "
        "-target bpf "
        "-Wall -Wextra "
        "-Iincludes"
    );
}

CF_CONFIG(base)
{
    CF_SET_ENV(cc, "cc");
    CF_SET_ENV(ar, "ar");
    CF_SET_ENV(ldflags, "");
}

CF_CONFIG(release)
{
    CF_CONFIG_EXTENDS(base);
    CF_CONFIG_EXTENDS(bpf);

    CF_SET_ENV(
        cflags,
        "-O2 "
        "-std=c11 "
        "-Wall -Wextra "
        "-Iincludes"
    );
}

CF_CONFIG(debug)
{
    CF_CONFIG_EXTENDS(base);
    CF_CONFIG_EXTENDS(bpf);

    CF_SET_ENV(
        cflags,
        "-O0 -g3 "
        "-std=c11 "
        "-Wall -Wextra "
        "-Iincludes"
    );
}

static void compile_c_dir(const char* src_glob, const char* build_dir)
{
    CF_MKDIR(build_dir);

    for CF_GLOBS_EACH(src_glob, in) {
        char* out = CF_MAP(in, CF_MAP_EXT("o"), CF_MAP_PARENT(BUILD_DIR));

        if (CF_FILE_NOT_UTD(in) || CF_FILE_NOT_UTD(out)) {
            printf(CC_TAG "%s\n", in);
            CF_RUNP("%s %s -c %s -o %s", CF_ENV(cc), CF_ENV(cflags), in, out);

            CF_FILE_MARK_UTDP(in);
            CF_FILE_MARK_UTDP(out);
        }
    }
}

CF_TARGET(
    all,
    CF_WITH_CONFIG(release),
    CF_DEPENDS(kmod),
    CF_DEPENDS(lib),
    CF_DEPENDS(xpd),
    CF_HELP_STRING("Build everything")
)
{
    CF_NOP();
}

CF_TARGET(clean, CF_HELP_STRING("Remove build artifacts"))
{
    CF_RM(BUILD_DIR);
}

CF_TARGET(
    kmod,
    CF_WITH_CONFIG(release),
    CF_HELP_STRING("Build the kernel module")
)
{
    bool rebuild = false;

    for CF_GLOBS_EACH(KMOD_DIR "/*", file) {
        if (CF_FILE_NOT_UTD(file)) {
            rebuild = true;
        }
    }

    if (CF_FILE_NOT_UTD(BUILD_DIR "/" KMOD_KO)) {
        rebuild = true;
    }

    if (!rebuild) {
        return;
    }

    printf(KO_TAG "%s\n", KMOD_KO);
    CF_MKDIR(BUILD_DIR);
    CF_RM(KMOD_BUILD_DIR);
    CF_CP(KMOD_DIR, KMOD_BUILD_DIR);

    cf_glob_t glob = CF_GLOB(KMOD_DIR "/*.c");
    char** objs = CF_MAPA(glob.p, glob.c, CF_MAP_EXT("o"), CF_MAP_DIRS(""));

    CF_WRITE(
        KMOD_BUILD_DIR "/Makefile",
        "ccflags-y := -I$(PWD)/includes\n"
        "obj-m += %s.o\n"
        "%s-y := %s\n",
        KMOD_NAME,
        KMOD_NAME,
        CF_JOIN(objs, " ", glob.c)
    );

    CF_RUN(
        "make -C /lib/modules/$(uname -r)/build M=$(pwd)/%s modules",
        KMOD_BUILD_DIR
    );

    CF_CP(KMOD_BUILD_DIR "/" KMOD_KO, BUILD_DIR "/" KMOD_KO);

    for CF_GLOBS_EACH(KMOD_DIR "/*", file) {
        CF_FILE_MARK_UTD(file);
    }

    CF_FILE_MARK_UTD(BUILD_DIR "/" KMOD_KO);
}

CF_TARGET(insert, CF_DEPENDS(remove), CF_HELP_STRING("Insert kernel module"))
{
    printf(KO_TAG "Inserting module: %s\n", KMOD_KO);
    CF_RUN("sudo insmod %s/%s", BUILD_DIR, KMOD_KO);
}

CF_TARGET(remove, CF_HELP_STRING("Remove kernel module"))
{
    const char* lsmod_log = BUILD_DIR "/lsmod.log";

    CF_MKDIR(BUILD_DIR);
    CF_RUN("lsmod | grep \"^%s\" > %s || true", KMOD_NAME, lsmod_log);
    char* ret = CF_READ(lsmod_log);
    if (ret == NULL || strlen(ret) == 0) {
        return;
    }

    printf(KO_TAG "Currently inserted: %s", ret);
    CF_RUN("sudo rmmod %s", KMOD_NAME);
}

CF_TARGET(
    lib,
    CF_WITH_CONFIG(release),
    CF_DEPENDS(lib_compile),
    CF_DEPENDS(lib_archive),
    CF_HELP_STRING("Build userspace static library")
)
{
    CF_NOP();
}

CF_TARGET(lib_compile, CF_HIDDEN)
{
    compile_c_dir(LIB_DIR "/*.c", LIB_BUILD_DIR);
}

CF_TARGET(lib_archive, CF_HIDDEN)
{
    if (CF_FILE_NOT_UTD(LIB)) {
        char* objs = CF_JOIN_GLOB(CF_GLOB(LIB_BUILD_DIR "/*.o"), " ");

        printf(AR_TAG "%s\n", LIB);
        CF_RUN("%s rcs %s %s", CF_ENV(ar), LIB, objs);

        CF_FILE_MARK_UTD(LIB);
    }
}

CF_TARGET(
    xpd,
    CF_WITH_CONFIG(release),
    CF_HELP_STRING("Build xpd XDP/eBPF object")
)
{
    bool rebuild = false;

    CF_MKDIR(BUILD_DIR);

    for CF_GLOBS_EACH(XPD_DIR "/*", file) {
        if (CF_FILE_NOT_UTD(file)) {
            rebuild = true;
        }
    }

    if (CF_FILE_NOT_UTD(XPD_OBJ)) {
        rebuild = true;
    }

    if (!rebuild) {
        return;
    }

    printf(CC_TAG "%s\n", XPD_DIR "/xpdfwd.c");

    CF_RUN(
        "%s %s -c %s/xpdfwd.c -o %s",
        CF_ENV(bpfcc),
        CF_ENV(bpfflags),
        XPD_DIR,
        XPD_OBJ
    );

    for CF_GLOBS_EACH(XPD_DIR "/*", file) {
        CF_FILE_MARK_UTD(file);
    }

    CF_FILE_MARK_UTD(XPD_OBJ);
}

CF_TARGET(load_xdp, CF_HELP_STRING("Load XDP program"))
{
    printf(CC_TAG "Loading XDP: %s on %s\n", XPD_OBJ, XPD_IFACE);

    CF_RUN(
        "sudo ip link set dev %s xdpgeneric obj %s sec %s",
        XPD_IFACE,
        XPD_OBJ,
        XPD_SECTION
    );
}

CF_TARGET(unload_xdp, CF_HELP_STRING("Unload XDP program"))
{
    printf(CC_TAG "Unloading XDP from %s\n", XPD_IFACE);

    CF_RUN("sudo ip link set dev %s xdpgeneric off", XPD_IFACE);
}
