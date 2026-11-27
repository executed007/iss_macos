/* sceSaveDataDirNameSearch by date (src/runtime_savedata.c). inFAMOUS writes its two autosaves within
 * the same second; the newest must still come first, or the game loads the older one and then
 * autosaves over the newer one. */
#define TLG_SYSTEM_MALLOC /* macOS: the runtime source below without the low heap (runtime_low_heap.c) */
#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include "../src/runtime_savedata.c"

static char user_dir[] = "/tmp/tlg-savedata-search-XXXXXX";
const char *runtime_file_user_dir(void) { return user_dir; }
int runtime_file_mount(const char *guest, const char *host) { (void)guest; (void)host; return 0; }
void runtime_file_unmount(const char *guest) { (void)guest; }
void tlggpu_save_toast(int kind) { (void)kind; }
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *name) {
    (void)table; (void)count; (void)name;
    return 0;
}

/* <user>/savedata/1/CUSA00004/<name>, its param.bin dated sec.nsec. */
static void save_at(const char *name, time_t sec, long nsec) {
    char path[700];
    snprintf(path, sizeof(path), "%s/savedata/1/CUSA00004/%s", user_dir, name);
    assert(!make_dirs(path));
    snprintf(path, sizeof(path), "%s/savedata/1/CUSA00004/%s.sce_sys", user_dir, name);
    assert(!make_dirs(path));
    Param p = {0};
    snprintf(p.subtitle, sizeof(p.subtitle), "%s", name);
    write_param(path, &p);
    strcat(path, "/param.bin");
    const struct timespec times[2] = {{sec, nsec}, {sec, nsec}};
    assert(!utimensat(AT_FDCWD, path, times, 0));
}

/* The first name of a newest-first date search, and its guest-visible date. */
static const char *newest(int64_t *mtime) {
    static DirName names[4];
    Param params[4];
    SearchCond cond = {0};
    cond.user = 1;
    cond.key = 3;   /* date */
    cond.order = 1; /* descending */
    SearchResult result = {0};
    result.names = names;
    result.names_capacity = 4;
    result.params = params;
    assert(save_search(&cond, &result) == 0 && result.hits == 2 && result.set_count == 2);
    *mtime = params[0].mtime;
    return names[0].data;
}

int main(void) {
    assert(mkdtemp(user_dir));
    runtime_savedata_configure("CUSA00004");
    assert(save_initialize(NULL) == 0);
    const time_t second = 1795792819; /* the player's two autosaves, 60 ms apart */
    int64_t mtime;

    save_at("infamousssAuto1", second, 463223355);
    save_at("infamousssAuto0", second, 522775998);
    assert(!strcmp(newest(&mtime), "infamousssAuto0"));
    assert(mtime == second); /* the game still sees whole seconds */

    /* The other way round, so directory order cannot pass the test by chance. */
    save_at("infamousssAuto1", second, 900000000);
    assert(!strcmp(newest(&mtime), "infamousssAuto1"));

    /* A later second wins over larger nanoseconds. */
    save_at("infamousssAuto0", second + 1, 0);
    assert(!strcmp(newest(&mtime), "infamousssAuto0") && mtime == second + 1);

    remove_tree(user_dir);
    puts("savedata search ok");
    return 0;
}
