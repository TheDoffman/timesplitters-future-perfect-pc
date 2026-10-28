/* Independent XBE image validation/materialization. Format vocabulary follows
 * the project's bounded tools/compiler/xbe_image.py reader. All disk integers
 * are decoded explicitly, including on unaligned input. No toolkit includes. */
#include "guest_image.h"
#include <string.h>

#define IMAGE_MAX_FILE (256u * 1024u * 1024u)
#define IMAGE_MAX_SECTIONS 4096u
#define IMAGE_MAX_IMPORTS 378u

typedef struct image_reader {
    const unsigned char *file;
    size_t size;
    uint32_t table;
    tsfp_image_info info;
} image_reader;

static uint32_t word(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int extent(uint64_t start, uint64_t size, uint64_t limit)
{
    return start <= limit && size <= limit - start;
}

static int intersects(uint64_t a, uint64_t an, uint64_t b, uint64_t bn)
{
    return an && bn && a < b + bn && b < a + an;
}

static const unsigned char *section(const image_reader *r, uint32_t index)
{
    return r->file + r->table + (size_t)index * 56u;
}

/* This is used only after all section descriptors have been validated. It
 * resolves initialized bytes, never BSS, alignment padding or image holes. */
static const unsigned char *backed(const image_reader *r, uint32_t va, uint32_t count)
{
    uint32_t i;
    if (va >= r->info.base && extent(va-r->info.base, count, r->info.header_size))
        return r->file + (va-r->info.base);
    for (i=0; i<r->info.section_count; ++i) {
        const unsigned char *s=section(r,i);
        uint32_t start=word(s+4), n=word(s+8), raw=word(s+16);
        if (raw<n) n=raw;
        if (va>=start && extent(va-start,count,n))
            return r->file + word(s+12) + (va-start);
    }
    return NULL;
}

static int mapped(const image_reader *r, uint32_t va, uint32_t count)
{
    uint32_t i;
    if (va>=r->info.base && extent(va-r->info.base,count,r->info.header_size)) return 1;
    for (i=0; i<r->info.section_count; ++i) {
        const unsigned char *s=section(r,i);
        uint32_t start=word(s+4);
        if (va>=start && extent(va-start,count,word(s+8))) return 1;
    }
    return 0;
}

static int executable(const image_reader *r, uint32_t va)
{
    uint32_t i;
    for (i=0; i<r->info.section_count; ++i) {
        const unsigned char *s=section(r,i);
        uint32_t start=word(s+4), n=word(s+8), raw=word(s+16);
        if (raw<n) n=raw;
        if ((word(s)&4u) && va>=start && va-start<n) return 1;
    }
    return 0;
}

static void title_utf8(char out[121], const unsigned char *in)
{
    size_t at=0;
    unsigned i;
    for (i=0; i<40; ++i) {
        uint32_t c=(uint32_t)in[2*i] | ((uint32_t)in[2*i+1]<<8);
        if (!c) break;
        if (c>=0xd800 && c<=0xdbff && i+1<40) {
            uint32_t next=(uint32_t)in[2*i+2] | ((uint32_t)in[2*i+3]<<8);
            if (next>=0xdc00 && next<=0xdfff) {
                c=0x10000+((c-0xd800)<<10)+(next-0xdc00);
                ++i;
            }
        }
        if (c>=0xd800 && c<=0xdfff) c=0xfffd;
        if (c<0x80) out[at++]=(char)c;
        else {
            if (c>=0x10000) out[at++]=(char)(0xf0|(c>>18));
            if (c>=0x800) out[at++]=(char)((c>=0x10000 ? 0x80 : 0xe0)|((c>>12)&0x3f));
            out[at++]=(char)((c>=0x800 ? 0x80 : 0xc0)|((c>>6)&0x3f));
            out[at++]=(char)(0x80|(c&0x3f));
        }
    }
    while (at && (out[at-1]==' ' || out[at-1]=='\t')) --at;
    out[at]=0;
}

static tsfp_image_result inspect(image_reader *r, size_t limit)
{
    const unsigned char *f=r->file, *p;
    uint32_t i,j,header_struct,table_va,cert_va,cert_size;
    uint64_t image_end;
    uint32_t candidate_count=0;
    static const uint32_t entry_keys[2]={0xa8fc57abu,0x94859d4bu};
    static const uint32_t thunk_keys[2]={0x5b6d40b6u,0xefb1f152u};
    if (r->size<0x178 || r->size>IMAGE_MAX_FILE || memcmp(f,"XBEH",4)) return TSFP_IMAGE_HEADER;
    r->info.base=word(f+0x104);
    r->info.header_size=word(f+0x108);
    image_end=(uint64_t)r->info.base+word(f+0x10c);
    header_struct=word(f+0x110);
    if (r->info.base<0x10000 || header_struct<0x178 || header_struct>r->info.header_size ||
        r->info.header_size>word(f+0x10c) || image_end>limit ||
        !extent(0,r->info.header_size,r->size)) return TSFP_IMAGE_HEADER;
    r->info.image_end=(uint32_t)image_end;
    r->info.section_count=word(f+0x11c);
    table_va=word(f+0x120);
    if (!r->info.section_count || r->info.section_count>IMAGE_MAX_SECTIONS || table_va<r->info.base ||
        !extent(table_va-r->info.base,(uint64_t)r->info.section_count*56,r->info.header_size))
        return TSFP_IMAGE_SECTION;
    r->table=table_va-r->info.base;
    for (i=0; i<r->info.section_count; ++i) {
        const unsigned char *s=section(r,i);
        uint32_t va=word(s+4), vn=word(s+8), raw=word(s+12), rn=word(s+16);
        if (va<(uint64_t)r->info.base+r->info.header_size || !extent(va,vn,image_end) ||
            (rn && (raw<r->info.header_size || !extent(raw,rn,r->size)))) return TSFP_IMAGE_SECTION;
        for (j=0; j<i; ++j) {
            const unsigned char *t=section(r,j);
            if (intersects(va,vn,word(t+4),word(t+8)) || intersects(raw,rn,word(t+12),word(t+16)))
                return TSFP_IMAGE_OVERLAP;
        }
        if (vn) {
            if (!r->info.image_lo || va<r->info.image_lo) r->info.image_lo=va;
            if (va+vn>r->info.image_hi) r->info.image_hi=va+vn;
            if (word(s)&4u) {
                if (!r->info.code_lo || va<r->info.code_lo) r->info.code_lo=va;
                if (va+vn>r->info.code_hi) r->info.code_hi=va+vn;
            }
        }
    }
    for (i=0; i<2; ++i) {
        uint32_t entry=word(f+0x128)^entry_keys[i], thunk=word(f+0x158)^thunk_keys[i];
        if (executable(r,entry) && (!thunk || backed(r,thunk,4))) {
            ++candidate_count;
            r->info.entry=entry; r->info.thunk=thunk; r->info.debug=i;
        }
    }
    if (candidate_count!=1) return TSFP_IMAGE_ENTRY;
    if (r->info.thunk) {
        for (i=0; i<=IMAGE_MAX_IMPORTS; ++i) {
            uint64_t address=(uint64_t)r->info.thunk+UINT64_C(4)*i;
            uint32_t ordinal;
            if (address>UINT32_MAX || !(p=backed(r,(uint32_t)address,4))) return TSFP_IMAGE_IMPORT;
            ordinal=word(p);
            if (!ordinal) break;
            if (i==IMAGE_MAX_IMPORTS || !(ordinal&0x80000000u) || !(ordinal&0x7fffffffu))
                return TSFP_IMAGE_IMPORT;
        }
        r->info.thunk_count=i;
    }
    cert_va=word(f+0x118);
    p=backed(r,cert_va,4);
    if (!p) return TSFP_IMAGE_CERTIFICATE;
    cert_size=word(p);
    if (cert_size<0x1d0 || cert_size>4096 || !(p=backed(r,cert_va,cert_size))) return TSFP_IMAGE_CERTIFICATE;
    r->info.title_id=word(p+8); r->info.region=word(p+0xa0);
    title_utf8(r->info.title,p+12);
    r->info.tls_directory=word(f+0x12c);
    if (r->info.tls_directory) {
        uint32_t end;
        p=backed(r,r->info.tls_directory,24);
        if (!p) return TSFP_IMAGE_TLS;
        r->info.tls_start=word(p); end=word(p+4);
        r->info.tls_index=word(p+8); r->info.tls_zero_fill=word(p+16);
        if (end<r->info.tls_start) return TSFP_IMAGE_TLS;
        r->info.tls_bytes=end-r->info.tls_start;
        if ((r->info.tls_bytes && !backed(r,r->info.tls_start,r->info.tls_bytes)) ||
            !mapped(r,r->info.tls_index,4) ||
            (uint64_t)r->info.tls_bytes+r->info.tls_zero_fill>UINT32_MAX-19u) return TSFP_IMAGE_TLS;
    }
    return TSFP_IMAGE_OK;
}

static int host_overlap(const void *a, size_t an, const void *b, size_t bn)
{
    uintptr_t av=(uintptr_t)a, bv=(uintptr_t)b;
    /* Treat unrepresentable host extents as invalid/overlapping. */
    if (an>UINTPTR_MAX-av || bn>UINTPTR_MAX-bv) return 1;
    return av<=bv ? bv-av<an : av-bv<bn;
}

tsfp_image_result tsfp_image_inspect(const void *file, size_t size,
                                    size_t guest_limit, tsfp_image_info *info)
{
    image_reader r;
    tsfp_image_result result;
    if (!file || !info || guest_limit>UINT32_MAX || host_overlap(file,size,info,sizeof *info))
        return TSFP_IMAGE_ARGUMENT;
    memset(&r,0,sizeof r); r.file=(const unsigned char *)file; r.size=size;
    result=inspect(&r,guest_limit);
    if (result==TSFP_IMAGE_OK) *info=r.info;
    return result;
}

tsfp_image_result tsfp_image_load(const void *file, size_t size, void *ram,
                                 size_t ram_size, tsfp_image_info *info)
{
    image_reader r;
    tsfp_image_result result;
    unsigned char *dest=(unsigned char *)ram;
    uint32_t i;
    if (!file || !ram || !info || ram_size>UINT32_MAX || host_overlap(file,size,info,sizeof *info))
        return TSFP_IMAGE_ARGUMENT;
    if (host_overlap(file,size,ram,ram_size) || host_overlap(info,sizeof *info,ram,ram_size))
        return TSFP_IMAGE_DESTINATION;
    memset(&r,0,sizeof r); r.file=(const unsigned char *)file; r.size=size;
    result=inspect(&r,ram_size);
    if (result!=TSFP_IMAGE_OK) return result;
    memcpy(dest+r.info.base,file,r.info.header_size);
    for (i=0; i<r.info.section_count; ++i) {
        const unsigned char *s=section(&r,i);
        uint32_t va=word(s+4), vn=word(s+8), rn=word(s+16), copy=rn<vn ? rn : vn;
        if (copy) memcpy(dest+va,r.file+word(s+12),copy);
        if (vn>copy) memset(dest+va+copy,0,vn-copy);
    }
    *info=r.info;
    return TSFP_IMAGE_OK;
}

tsfp_image_result tsfp_image_bootstrap(const tsfp_image_info *info, size_t limit,
                                      uint32_t *low_shift, uint32_t *tls_total)
{
    uint64_t shift=0, total=0;
    if (!info || !low_shift || !tls_total || limit>UINT32_MAX || low_shift==tls_total)
        return TSFP_IMAGE_ARGUMENT;
    if (info->image_end>0x700000u)
        shift=((uint64_t)info->image_end-0x700000u+0xffffu)&~UINT64_C(0xffff);
    if (shift+0xf80000u>=limit) return TSFP_IMAGE_DESTINATION;
    if (info->tls_directory) {
        total=(((uint64_t)info->tls_bytes+info->tls_zero_fill+15u)&~UINT64_C(15))+4u;
        if (total>0x200u) return TSFP_IMAGE_TLS;
    }
    *low_shift=(uint32_t)shift; *tls_total=(uint32_t)total;
    return TSFP_IMAGE_OK;
}

const char *tsfp_image_result_name(tsfp_image_result result)
{
    static const char *const names[]={"ok","argument","header","section","overlap",
        "entry","import","certificate","tls","destination"};
    return (unsigned)result<sizeof names/sizeof names[0] ? names[result] : "unknown";
}
