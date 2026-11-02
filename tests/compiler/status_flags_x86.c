/* Real x86 arithmetic flags; native control flags remain untouched. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <io.h>
#include <fcntl.h>
static uint32_t g_a,g_b,g_seed,g_result,g_status;
static void execute(unsigned op,unsigned bits) {
    if(op==17) {
        __asm {
            pushfd
            pop edx
            and edx,0fffff72ah
            mov eax,g_seed
            and eax,08d5h
            or edx,eax
            push edx
            popfd
            lahf
            movzx eax,ah
            mov g_result,eax
            pushfd
            pop g_status
        }
        return;
    }
    if(bits==8)switch(op) {
    case 0: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        add al,cl
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 1: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        sub al,cl
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 2: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        adc al,cl
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 3: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        sbb al,cl
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 4: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        inc al
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 5: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        dec al
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 6: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        neg al
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 7: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        and al,cl
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 8: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        or al,cl
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 9: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        xor al,cl
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 10: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        cmp al,cl
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 11: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        test al,cl
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    }
    if(bits==16)switch(op) {
    case 0: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        add ax,cx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 1: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        sub ax,cx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 2: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        adc ax,cx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 3: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        sbb ax,cx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 4: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        inc ax
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 5: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        dec ax
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 6: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        neg ax
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 7: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        and ax,cx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 8: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        or ax,cx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 9: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        xor ax,cx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 10: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        cmp ax,cx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 11: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        test ax,cx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    }
    if(bits==32)switch(op) {
    case 0: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        add eax,ecx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 1: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        sub eax,ecx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 2: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        adc eax,ecx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 3: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        sbb eax,ecx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 4: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        inc eax
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 5: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        dec eax
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 6: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        neg eax
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 7: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        and eax,ecx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 8: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        or eax,ecx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 9: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        xor eax,ecx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 10: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        cmp eax,ecx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    case 11: __asm {
        pushfd
        pop edx
        and edx,0fffff72ah
        mov eax,g_seed
        and eax,08d5h
        or edx,eax
        push edx
        popfd
        mov eax,g_a
        mov ecx,g_b
        test eax,ecx
        mov g_result,eax
        pushfd
        pop g_status
    } break;
    }
}
int main(void) {
    uint32_t p[7],out[2];assert(_setmode(_fileno(stdin),_O_BINARY)!=-1);assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);
    while(fread(p,1,sizeof(p),stdin)==sizeof(p)) {
        unsigned op=p[0],bits=p[1];uint32_t mask=bits==32?UINT32_MAX:(UINT32_C(1)<<bits)-1;
        assert((op<12 || op==17) && (bits==8 || bits==16 || bits==32));g_a=p[2];g_b=p[3];g_seed=p[5];
        if(op==2 || op==3)g_seed=(g_seed&~1u)|((p[6]&1)?p[5]&1u:!!p[4]);
        if(op==17)g_seed=(g_seed&~0xd5u)|((p[5]&p[6]|g_a&~p[6])&0xd5u);
        execute(op,bits);out[0]=g_result&mask;out[1]=g_status;assert(fwrite(out,1,sizeof(out),stdout)==sizeof(out));
    }
    return 0;
}
