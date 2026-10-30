/* Original Gregorian arithmetic and byte serialization. */
#include "guest_time.h"
#include <stdint.h>
#include <string.h>
static unsigned time_read16(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1]<<8);
}
static void time_write16(unsigned char *p,unsigned value) {
    p[0]=(unsigned char)value;p[1]=(unsigned char)(value>>8);
}
static int time_leap(unsigned year) {
    return year%4==0 && (year%100!=0 || year%400==0);
}
static unsigned time_month_days(unsigned year,unsigned month) {
    static const unsigned char lengths[12]={31,28,31,30,31,30,31,31,30,31,30,31};
    return lengths[month-1]+(unsigned)(month==2 && time_leap(year));
}
static uint64_t time_year_days(unsigned year) {
    uint64_t previous=year-1;
    return (uint64_t)(year-1601)*365+(previous/4-400)
        -(previous/100-16)+(previous/400-4);
}
int tsfp_time_fields_to_ticks(const void *input,size_t bytes,void *output,size_t capacity) {
    const unsigned char *p=(const unsigned char *)input;
    unsigned year,month,day,hour,minute,second,ms,m,i;
    uint64_t days,ticks;unsigned char result[8];
    if(!input || !output || bytes<16 || capacity<8)return 0;
    year=time_read16(p);month=time_read16(p+2);day=time_read16(p+4);
    hour=time_read16(p+6);minute=time_read16(p+8);second=time_read16(p+10);ms=time_read16(p+12);
    if(year<1601 || year>30827 || month<1 || month>12 || day<1 ||
       hour>23 || minute>59 || second>59 || ms>999)return 0;
    if(day>time_month_days(year,month))return 0;
    days=time_year_days(year)+day-1;
    for(m=1;m<month;++m)days+=time_month_days(year,m);
    ticks=((days*86400+hour*3600+minute*60+second)*1000+ms)*10000;
    for(i=0;i<8;++i)result[i]=(unsigned char)(ticks>>(8*i));
    memcpy(output,result,sizeof(result));return 1;
}
int tsfp_time_ticks_to_fields(const void *input,size_t bytes,void *output,size_t capacity) {
    const unsigned char *p=(const unsigned char *)input;
    uint64_t ticks=0,days,remaining;unsigned low,high,mid,month,i,weekday;
    unsigned char result[16];
    if(!input || !output || bytes<8 || capacity<16)return 0;
    for(i=0;i<8;++i)ticks|=(uint64_t)p[i]<<(8*i);
    if(ticks>>63)return 0;
    remaining=ticks/10000;days=remaining/86400000;remaining%=86400000;
    weekday=(unsigned)((days+1)%7);
    low=1601;high=1602+(unsigned)(days/365);
    while(high-low>1) {
        mid=low+(high-low)/2;
        if(time_year_days(mid)<=days)low=mid;else high=mid;
    }
    days-=time_year_days(low);month=1;
    while(days>=time_month_days(low,month)){days-=time_month_days(low,month);++month;}
    time_write16(result,low);time_write16(result+2,month);time_write16(result+4,(unsigned)days+1);
    time_write16(result+6,(unsigned)(remaining/3600000));remaining%=3600000;
    time_write16(result+8,(unsigned)(remaining/60000));remaining%=60000;
    time_write16(result+10,(unsigned)(remaining/1000));time_write16(result+12,(unsigned)(remaining%1000));
    time_write16(result+14,weekday);memcpy(output,result,sizeof(result));return 1;
}
