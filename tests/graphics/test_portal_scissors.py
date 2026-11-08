"""Guest inclusive scissors retain coverage at fractional render scales."""
from pathlib import Path
import pytest
from test_cutscene_letterbox import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize('mutation', ['none', 'exclusive', 'inward'])
def test_inclusive_scaled_bounds(tmp_path, mutation):
    header = (ROOT/'src/graphics/guest_scissor.h').read_text()
    if mutation == 'exclusive':
        header = header.replace('(double)rect[i+2]+1.0', '(double)rect[i+2]')
    elif mutation == 'inward':
        header = header.replace('ceil(hi)', 'floor(hi)')
    compile_and_run(tmp_path, header + r'''
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL: %d\n",__LINE__);return 1;}}while(0)
int main(void) {
 int32_t r[4]={201,110,375,321},saved[4],b[4];memcpy(saved,r,sizeof r);
 pc_guest_scissor(r,2,1.5,1,1,0,0,b);
 CHECK(b[0]==402 && b[1]==165 && b[2]==752 && b[3]==483);
 CHECK(!memcmp(saved,r,sizeof r));
 pc_guest_scissor(r,1366./640,768./480,1,1,0,0,b);
 CHECK(b[0]==429 && b[1]==176 && b[2]==803 && b[3]==516);
 int32_t pixel[4]={3,4,3,4};pc_guest_scissor(pixel,1,1,1,1,0,0,b);
 CHECK(b[0]==3 && b[1]==4 && b[2]==4 && b[3]==5);
 int32_t negative[4]={-3,-4,-3,-4};pc_guest_scissor(negative,.5,.5,1,1,0,0,b);
 CHECK(b[0]==-2 && b[1]==-2 && b[2]==-1 && b[3]==-1);
 int32_t empty[4]={3,4,2,3};pc_guest_scissor(empty,1,1,1,1,0,0,b);
 CHECK(b[0]==b[2] && b[1]==b[3]);
 int32_t large[4]={INT32_MIN,INT32_MIN,INT32_MAX,INT32_MAX};
 pc_guest_scissor(large,16,16,1,1,0,0,b);
 CHECK(b[0]==INT32_MIN && b[2]==INT32_MAX);
 puts("PASS: guest inclusive/empty/scaled/saturated bounds");return 0;
}
''', fails=mutation != 'none')
