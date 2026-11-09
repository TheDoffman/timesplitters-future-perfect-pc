#include "interface/pc_update_notice.h"
#include <assert.h>
#include <limits.h>
static int check(const char *s,const char *installed,long long now) {
    return pc_update_notice_available(s,strlen(s),installed,now);
}
int main(void) {
    const char *valid="TSFP_GAME_UPDATE_V1 100000 v1.2.3 v1.2.4\n";
    assert(check(valid,"v1.2.3",100000));
    assert(check(valid,"1.2.3",100000));
    assert(!check(valid,"v1.2.4",100000));
    assert(!check(valid,"development",100000));
    assert(check(valid,"1.2.3",186400));
    assert(!check(valid,"1.2.3",186401));
    assert(check(valid,"1.2.3",99700));
    assert(!check(valid,"1.2.3",99699));
    assert(!check(valid,"1.2.3",LLONG_MAX));
    assert(!check("TSFP_GAME_UPDATE_V1 100000 v1.2.3 1.2.3","1.2.3",100000));
    assert(!check("TSFP_GAME_UPDATE_V1 100000 v1.2.3 v1.2.4 extra","1.2.3",100000));
    assert(!check("TSFP_GAME_UPDATE_V1 100000 v1.2.3 <unsafe>","1.2.3",100000));
    assert(!check("TSFP_GAME_UPDATE_V2 100000 v1.2.3 v1.2.4","1.2.3",100000));
    assert(!check("TSFP_GAME_UPDATE_V1 -1 v1.2.3 v1.2.4","1.2.3",100000));
    assert(!pc_update_notice_available(valid,strlen(valid)+1,"1.2.3",100000));
    char oversized[512];memset(oversized,' ',sizeof oversized);
    assert(!pc_update_notice_available(oversized,sizeof oversized,"1.2.3",100000));
    assert(!check("","1.2.3",100000));
    puts("PASS: bounded main menu update result validation");
    return 0;
}
