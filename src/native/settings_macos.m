#import <Cocoa/Cocoa.h>
#include "settings_macos.h"
#include "pc_settings.h"
#include "pc_display.h"
#include "pc_debug.h"
#include "config_files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static SDL_Window *game_window;
static PcDebugOptions debug_options;
void pc_debug_set(const PcDebugOptions *s) { debug_options=*s; }
static void alert_text(const char *text) {
    NSAlert *alert=[NSAlert new];alert.messageText=@"TimeSplitters Settings";
    alert.informativeText=[NSString stringWithUTF8String:text];[alert runModal];
}
static NSTextField *label(NSView *view,NSString *text,NSRect frame) {
    NSTextField *l=[NSTextField labelWithString:text];l.frame=frame;[view addSubview:l];return l;
}
@interface TSFPSettings : NSObject <NSWindowDelegate> {
    NSWindow *panel;
    NSTabView *tabs;
    NSPopUpButton *bindings[PC_ACTION_COUNT][PC_BINDING_SLOTS];
    NSTextField *sensitivity;
    NSButton *enabled,*invert,*fullscreen;
    NSPopUpButton *resolution;
    PcDisplayMode modes[65];int mode_count;
    PcSettings original;
    int keys[256],key_count;
}
- (void)show;
- (void)save:(id)sender;
- (void)cancel:(id)sender;
- (void)defaults:(id)sender;
@end
static TSFPSettings *settings;
@implementation TSFPSettings
- (NSButton *)button:(NSString *)title action:(SEL)action frame:(NSRect)frame view:(NSView *)view {
    NSButton *b=[NSButton buttonWithTitle:title target:self action:action];b.frame=frame;[view addSubview:b];return b;
}
- (void)fill:(const PcSettings *)s {
    for(int a=0;a<PC_ACTION_COUNT;a++)for(int slot=0;slot<PC_BINDING_SLOTS;slot++) {
        int found=-1;for(int k=0;k<key_count;k++)if(keys[k]==s->keys[a][slot])found=k;
        if(found<0){char text[80];pc_settings_key_name(s->keys[a][slot],text,sizeof text);
            [bindings[a][slot] addItemWithTitle:[NSString stringWithUTF8String:text]];
            bindings[a][slot].lastItem.representedObject=@(s->keys[a][slot]);
            [bindings[a][slot] selectItem:bindings[a][slot].lastItem];
        } else [bindings[a][slot] selectItemAtIndex:found];
    }
    sensitivity.stringValue=[NSString stringWithFormat:@"%.4g",s->sensitivity];enabled.state=s->enabled;invert.state=s->invert_y;
}
- (void)show {
    if(panel){[panel makeKeyAndOrderFront:nil];return;}
    char error[512];if(!pc_settings_read(&original,error,sizeof error)){alert_text(error);return;}
    tsfp_native_settings_release_input(game_window);
    panel=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,860,650)
        styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable backing:NSBackingStoreBuffered defer:NO];
    panel.title=@"TimeSplitters — Mac Settings";panel.delegate=self;panel.releasedWhenClosed=NO;
    NSView *view=panel.contentView;
    tabs=[[NSTabView alloc] initWithFrame:NSMakeRect(15,60,830,575)];[view addSubview:tabs];
    NSTabViewItem *controls=[[NSTabViewItem alloc] initWithIdentifier:@"controls"];controls.label=@"Keyboard & Mouse";
    NSView *cv=[[NSView alloc] initWithFrame:NSMakeRect(0,0,805,535)];controls.view=cv;[tabs addTabViewItem:controls];
    label(cv,@"Choose up to four bindings per action. Settings apply when saved.",NSMakeRect(10,500,780,22));
    NSScrollView *scroll=[[NSScrollView alloc] initWithFrame:NSMakeRect(5,110,800,385)];scroll.hasVerticalScroller=YES;
    NSView *rows=[[NSView alloc] initWithFrame:NSMakeRect(0,0,780,PC_ACTION_COUNT*32)];scroll.documentView=rows;[cv addSubview:scroll];
    key_count=0;keys[key_count++]=0;
    for(int k=1;k<256;k++) {
        if(k==0x70||k==0x71||(k>=0x78&&k<=0x7b)||k==0x5b||k==0x5c)continue;
        char text[80];tsfp_native_key_name(k,text,sizeof text);
        if(strncmp(text,"Key ",4))keys[key_count++]=k;
    }
    for(int a=0;a<PC_ACTION_COUNT;a++) {
        CGFloat y=(PC_ACTION_COUNT-a-1)*32;
        label(rows,[NSString stringWithUTF8String:pc_action_labels[a]],NSMakeRect(5,y+4,270,24));
        for(int slot=0;slot<PC_BINDING_SLOTS;slot++) {
            NSPopUpButton *b=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(280+slot*122,y,120,28) pullsDown:NO];bindings[a][slot]=b;
            for(int k=0;k<key_count;k++){char text[80];pc_settings_key_name(keys[k],text,sizeof text);[b addItemWithTitle:[NSString stringWithUTF8String:text]];b.lastItem.representedObject=@(keys[k]);}
            [rows addSubview:b];
        }
    }
    [rows scrollPoint:NSMakePoint(0,PC_ACTION_COUNT*32)];
    label(cv,@"Mouse sensitivity",NSMakeRect(10,75,145,24));
    sensitivity=[[NSTextField alloc] initWithFrame:NSMakeRect(160,75,100,24)];[cv addSubview:sensitivity];
    enabled=[NSButton checkboxWithTitle:@"Mouse look enabled" target:nil action:NULL];enabled.frame=NSMakeRect(285,75,200,24);[cv addSubview:enabled];
    invert=[NSButton checkboxWithTitle:@"Invert vertical look" target:nil action:NULL];invert.frame=NSMakeRect(510,75,240,24);[cv addSubview:invert];
    label(cv,@"F1 releases/captures the pointer. F2 opens Settings. Reserved shortcuts cannot be rebound.",NSMakeRect(10,35,785,30));
    [self fill:&original];
    NSTabViewItem *graphics=[[NSTabViewItem alloc] initWithIdentifier:@"graphics"];graphics.label=@"Graphics";
    NSView *gv=[[NSView alloc] initWithFrame:cv.frame];graphics.view=gv;[tabs addTabViewItem:graphics];
    label(gv,@"Render resolution",NSMakeRect(25,450,180,25));
    resolution=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(220,447,340,30) pullsDown:NO];[gv addSubview:resolution];
    modes[0]=(PcDisplayMode){0,0};mode_count=1+pc_display_modes(game_window,modes+1,64);
    PcDisplayMode current=pc_display_current();
    for(int i=0;i<mode_count;i++){[resolution addItemWithTitle:i?[NSString stringWithFormat:@"%u × %u",modes[i].width,modes[i].height]:@"Original (640 × 480)"];if(modes[i].width==current.width&&modes[i].height==current.height)[resolution selectItemAtIndex:i];}
    fullscreen=[NSButton checkboxWithTitle:@"Fullscreen" target:nil action:NULL];fullscreen.frame=NSMakeRect(25,390,300,30);fullscreen.state=!!(SDL_GetWindowFlags(game_window)&SDL_WINDOW_FULLSCREEN);[gv addSubview:fullscreen];
    label(gv,@"Higher resolutions increase GPU load. The 60 Hz game clock stays unchanged.\nResolution changes preserve the original menus and widen the gameplay camera.\n\nUse the game's Display settings entry for a timed resolution preview.\nTexture packs can be selected with the launcher’s --texture-pack option.",NSMakeRect(25,180,740,160));
    [self button:@"Defaults" action:@selector(defaults:) frame:NSMakeRect(20,15,110,32) view:view];
    [self button:@"Cancel" action:@selector(cancel:) frame:NSMakeRect(610,15,100,32) view:view];
    NSButton *save=[self button:@"Save this tab" action:@selector(save:) frame:NSMakeRect(720,15,120,32) view:view];save.keyEquivalent=@"\r";
    [panel center];[panel makeKeyAndOrderFront:nil];
    [NSApp runModalForWindow:panel];[panel orderOut:nil];panel=nil;
    SDL_RaiseWindow(game_window);
    tsfp_native_settings_release_input(game_window);
}
- (void)save:(id)sender {
    char error[512];
    if([tabs.selectedTabViewItem.identifier isEqual:@"controls"]) {
        PcSettings next=original;
        for(int a=0;a<PC_ACTION_COUNT;a++)for(int s=0;s<PC_BINDING_SLOTS;s++)next.keys[a][s]=[bindings[a][s].selectedItem.representedObject intValue];
        for(int a=0;a<PC_ACTION_COUNT;a++)for(int slot=0;slot<PC_BINDING_SLOTS;slot++) {
            int conflict=pc_settings_conflict(&next,a,next.keys[a][slot]);
            if(conflict>=0){snprintf(error,sizeof error,"%s and %s share a binding. Set one to Unbound before saving.",pc_action_labels[a],pc_action_labels[conflict]);alert_text(error);return;}
        }
        next.sensitivity=sensitivity.doubleValue;next.enabled=enabled.state==NSControlStateValueOn;next.invert_y=invert.state==NSControlStateValueOn;
        if(!pc_settings_save(&next,error,sizeof error)){alert_text(error);return;}
    } else {
        PcDisplayMode before=pc_display_current(),next=modes[resolution.indexOfSelectedItem];
        if(!pc_display_apply(next)){alert_text("The selected render resolution could not be allocated. Your previous resolution is unchanged.");return;}
        if(!pc_display_save()){pc_display_apply(before);alert_text("Could not save the display settings. The previous resolution was restored.");return;}
        if(!SDL_SetWindowFullscreen(game_window,fullscreen.state==NSControlStateValueOn))alert_text(SDL_GetError());
    }
    [NSApp stopModal];
}
- (void)cancel:(id)sender {[NSApp stopModal];}
- (BOOL)windowShouldClose:(NSWindow *)sender {[NSApp stopModal];return NO;}
- (void)defaults:(id)sender {
    if([tabs.selectedTabViewItem.identifier isEqual:@"controls"]){PcSettings value;pc_settings_defaults(&value);[self fill:&value];}
    else {[resolution selectItemAtIndex:0];fullscreen.state=NSControlStateValueOff;}
}
@end
@interface TSFPMenu : NSObject
- (void)settings:(id)sender;
- (void)fullscreen:(id)sender;
@end
@implementation TSFPMenu
- (void)settings:(id)sender {tsfp_native_settings_show(game_window);}
- (void)fullscreen:(id)sender {tsfp_native_settings_release_input(game_window);SDL_SetWindowFullscreen(game_window,!(SDL_GetWindowFlags(game_window)&SDL_WINDOW_FULLSCREEN));}
@end
static TSFPMenu *menu_target;
void tsfp_native_settings_attach(SDL_Window *window) {
    game_window=window;if(menu_target)return;menu_target=[TSFPMenu new];
    NSMenu *menu=[[NSMenu alloc] initWithTitle:@"Game Settings"];
    NSMenuItem *settings_item=[[NSMenuItem alloc] initWithTitle:@"Keyboard, Mouse & Graphics…" action:@selector(settings:) keyEquivalent:@","];
    settings_item.target=menu_target;[menu addItem:settings_item];
    NSMenuItem *full=[[NSMenuItem alloc] initWithTitle:@"Toggle Fullscreen" action:@selector(fullscreen:) keyEquivalent:@"f"];
    full.keyEquivalentModifierMask=NSEventModifierFlagCommand|NSEventModifierFlagControl;full.target=menu_target;[menu addItem:full];
    NSMenuItem *top=[[NSMenuItem alloc] initWithTitle:@"Game Settings" action:NULL keyEquivalent:@""];top.submenu=menu;[NSApp.mainMenu addItem:top];
}
void tsfp_native_settings_show(SDL_Window *window) {
    game_window=window;if(!settings)settings=[TSFPSettings new];[settings show];
}
