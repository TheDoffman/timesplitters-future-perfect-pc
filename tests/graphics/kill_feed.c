#include "interface/pc_kill_feed.h"
#include <stdlib.h>
#include "interface/pc_feed_attribution.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
 PcFeedAttribution a={0};pc_feed_attribution_scene(&a,100,1);
 pc_feed_shot(&a,0x1000,20,3,35);pc_feed_shot(&a,0x2000,20,4,41);
 pc_feed_hit(&a,0x8000,0x1000,20,3,100);
 CHECK(pc_feed_hit_icon(&a,0x8000,20,3,100)==35); /* weapon switch preserves old projectile */
 CHECK(!pc_feed_hit_icon(&a,0x8000,20,3,101));
 pc_feed_shot(&a,0x1000,0,0,0);pc_feed_hit(&a,0x8000,0x1000,20,3,102);
 CHECK(!pc_feed_hit_icon(&a,0x8000,20,3,102)); /* recycled allocation */
 pc_feed_shot(&a,0x1000,20,3,35);pc_feed_shot(&a,0x5000,20,3,41);
 pc_feed_hit(&a,0x8000,0x1000,20,3,103);CHECK(!pc_feed_hit_icon(&a,0x8000,20,3,103));
 CHECK(pc_feed_color(1,2,0,2,1)==0x305b7f00u);
 CHECK(pc_feed_color(1,2,0,2,0)==0x7f303000u);
 CHECK(pc_feed_color(0,0,0,0,0)==0x305b7f00u);
 CHECK(pc_feed_color(1,2,1,2,1)==0x305b7f00u);
 CHECK(pc_feed_color(1,2,16,0,1)==0x707f7f00u);
 PcKillFeed f={0};PcFeedSnapshot s={0};s.pool=100;s.match=1;s.count=3;
 pc_feed_name(s.names[0],"Cortez");pc_feed_name(s.names[1],"Harry Tipper");pc_feed_name(s.names[2],"Monkey");
 s.kills[0][1]=8;pc_feed_sample(&f,&s,0,1);CHECK(!f.count); /* late join baseline */
 s.weapons[0][1]=35;s.teams[0]=1;s.teams[1]=2;
 s.kills[0][1]++;pc_feed_sample(&f,&s,1,1);CHECK(f.count==1 && strstr(f.rows[0].text,"Cortez > Harry"));
 CHECK(f.rows[0].weapon==35 && f.rows[0].killer_team==1 && f.rows[0].victim_team==2);
 pc_feed_sample(&f,&s,2,1);CHECK(f.count==1); /* retransmit/split view */
 s.kills[0][1]--;pc_feed_sample(&f,&s,3,1);s.kills[0][1]++;pc_feed_sample(&f,&s,4,1);CHECK(f.count==1);
 s.kills[2][0]++;s.kills[2][2]++;pc_feed_sample(&f,&s,5,1);CHECK(f.count==3 && strstr(f.rows[2].text,"[self]"));
 CHECK(pc_feed_alpha(&f.rows[2],4005)==127 && pc_feed_alpha(&f.rows[2],4505)==63);
 pc_feed_expire(&f,5005);CHECK(!f.count);
 s.weapons[1][2]=41;
 s.kills[1][2]+=30;pc_feed_sample(&f,&s,5010,1);CHECK(f.count==5);CHECK(f.rows[4].weapon==41 && !f.rows[3].weapon);
 pc_feed_sample(&f,&s,5020,0);CHECK(!f.count && !f.ready);pc_feed_sample(&f,&s,5030,1);CHECK(!f.count);
 s.match++;pc_feed_sample(&f,&s,5040,1);CHECK(!f.count);
 pc_feed_name(s.names[0],"Changed");s.kills[0][1]++;pc_feed_sample(&f,&s,5050,1);CHECK(!f.count);
 s.kills[0][1]=65535;f.previous.kills[0][1]=65535;s.kills[0][1]=0;pc_feed_sample(&f,&s,5060,1);CHECK(f.count==1);
 char name[32];pc_feed_name(name,"a\vb\nc");CHECK(!strcmp(name,"abc"));
 s.count=17;pc_feed_sample(&f,&s,5070,1);CHECK(!f.ready);
 puts("PASS: confirmed kills, bots, replay suppression, match/roster resets, capacity, fade and toggling");return 0;
}
