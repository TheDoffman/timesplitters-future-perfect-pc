#ifndef TSFP_DEVELOPER_FONT_H
#define TSFP_DEVELOPER_FONT_H
#include <stddef.h>
enum { PC_FONT_UI,PC_FONT_MONO };
typedef struct PcDeveloperFontMetrics {float advance,ascent,descent;} PcDeveloperFontMetrics;
PcDeveloperFontMetrics pc_developer_font_measure(const char *,float,unsigned,unsigned);
PcDeveloperFontMetrics pc_developer_font_measure_spaced(const char *,float,unsigned,unsigned,float);
int pc_developer_font_bitmap(const char *,unsigned char *,unsigned,unsigned,unsigned *,float,unsigned,unsigned,float);
void pc_developer_font_fit_spaced(const char *,float,unsigned,unsigned,float,float,char *,size_t);
void pc_developer_font_fit(const char *,float,unsigned,unsigned,float,char *,size_t);
#endif
