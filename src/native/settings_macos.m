#import <Cocoa/Cocoa.h>
#include "settings_macos.h"
#include "pc_storage.h"
#include "crash_posix.h"
#include <stdatomic.h>
#include "pc_settings.h"
#include "pc_display.h"
#include "pc_debug.h"
#include "config_files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "binding_capture.inc"
static SDL_Window *game_window;
static atomic_int save_picker_state;
static char save_picker_path[4096];
int pc_storage_picker_start(void) {
    int expected=0;if(!atomic_compare_exchange_strong(&save_picker_state,&expected,1))return 0;
    dispatch_async(dispatch_get_main_queue(), ^{
        tsfp_native_settings_release_input(game_window);tsfp_native_crash_inhibit(1);
        NSOpenPanel *picker=[NSOpenPanel openPanel];picker.canChooseDirectories=YES;picker.canChooseFiles=NO;picker.canCreateDirectories=YES;picker.allowsMultipleSelection=NO;
        picker.message=@"Choose an empty folder or your existing TimeSplitters saves. Existing files stay where they are.";
        [picker beginWithCompletionHandler:^(NSModalResponse response){
            int accepted=0;const char *path=picker.URL.path.UTF8String;
            if(response==NSModalResponseOK && path && strlen(path)<sizeof save_picker_path){snprintf(save_picker_path,sizeof save_picker_path,"%s",path);accepted=1;}
            tsfp_native_settings_release_input(game_window);tsfp_native_crash_inhibit(0);atomic_store(&save_picker_state,accepted?2:3);
        }];
    });return 1;
}
int pc_storage_picker_poll(char *out,size_t capacity) {
    int state=atomic_load(&save_picker_state);if(state<2)return 0;
    if(state==2)snprintf(out,capacity,"%s",save_picker_path);
    atomic_store(&save_picker_state,0);return state==2?1:-1;
}
static char *settings_test_error;
static void alert_text(const char *text) {
    if(settings_test_error){snprintf(settings_test_error,512,"%s",text);return;}
    NSAlert *alert=[NSAlert new];alert.messageText=@"TimeSplitters Settings";
    alert.informativeText=[NSString stringWithUTF8String:text];[alert runModal];
}
static NSTextField *label(NSView *view,NSString *text,NSRect frame) {
    NSTextField *l=[NSTextField labelWithString:text];l.frame=frame;[view addSubview:l];return l;
}
#include "debug_macos.inc"
@interface TSFPSettings : NSObject <NSWindowDelegate> {
    NSWindow *panel;
    NSTabView *tabs;
    NSPopUpButton *bindings[PC_ACTION_COUNT][PC_BINDING_SLOTS];
    NSTextField *sensitivity;
    NSButton *enabled,*invert,*fullscreen;
    NSPopUpButton *resolution,*aspect;
    NSSlider *fov;
    NSTextField *fov_value;
    PcDisplayMode all_modes[PC_DISPLAY_MODE_CAPACITY],modes[PC_DISPLAY_MODE_CAPACITY];
    int all_mode_count,mode_count;
    PcSettings original;
    int keys[256],key_count;
}
- (void)show;
- (void)save:(id)sender;
- (void)cancel:(id)sender;
- (void)defaults:(id)sender;
- (void)record:(id)sender;
- (void)aspectChanged:(id)sender;
- (void)fovChanged:(id)sender;
- (void)fillModes:(PcDisplayMode)staged;
- (PcSettings)bindingValues;
@end
static TSFPSettings *settings;
static void (^graphics_test_ready)(TSFPSettings *,NSTabView *,NSPopUpButton *,NSPopUpButton *,NSSlider *);
@implementation TSFPSettings
- (NSButton *)button:(NSString *)title action:(SEL)action frame:(NSRect)frame view:(NSView *)view {
    NSButton *b=[NSButton buttonWithTitle:title target:self action:action];b.frame=frame;[view addSubview:b];return b;
}
- (void)fillBindings:(const PcSettings *)s {
    for(int a=0;a<PC_ACTION_COUNT;a++)for(int slot=0;slot<PC_BINDING_SLOTS;slot++) {
        NSMenuItem *found=nil;
        for(NSMenuItem *item in bindings[a][slot].itemArray)
            if([item.representedObject intValue]==s->keys[a][slot]){found=item;break;}
        if(!found){char text[80];pc_settings_key_name(s->keys[a][slot],text,sizeof text);
            NSString *name=[NSString stringWithFormat:@"%s (unavailable on Mac)",text];
            [bindings[a][slot] addItemWithTitle:name];found=bindings[a][slot].lastItem;
            found.representedObject=@(s->keys[a][slot]);
        }
        [bindings[a][slot] selectItem:found];
    }
}
- (void)fill:(const PcSettings *)s {
    [self fillBindings:s];
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
    label(cv,@"Choose a binding or use Record to press a key, mouse button or wheel. Save applies this tab.",NSMakeRect(10,500,780,22));
    NSScrollView *scroll=[[NSScrollView alloc] initWithFrame:NSMakeRect(5,110,800,385)];scroll.hasVerticalScroller=YES;
    NSView *rows=[[NSView alloc] initWithFrame:NSMakeRect(0,0,780,PC_ACTION_COUNT*32)];scroll.documentView=rows;[cv addSubview:scroll];
    key_count=0;keys[key_count++]=0;
    for(int k=1;k<256;k++) {
        if(!pc_settings_key_allowed(k))continue;
        char text[80];tsfp_native_key_name(k,text,sizeof text);
        if(strncmp(text,"Key ",4))keys[key_count++]=k;
    }
    for(int a=0;a<PC_ACTION_COUNT;a++) {
        CGFloat y=(PC_ACTION_COUNT-a-1)*32;
        label(rows,[NSString stringWithUTF8String:pc_action_labels[a]],NSMakeRect(5,y+4,180,24));
        for(int slot=0;slot<PC_BINDING_SLOTS;slot++) {
            NSPopUpButton *b=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(190+slot*146,y,87,28) pullsDown:NO];bindings[a][slot]=b;
            for(int k=0;k<key_count;k++){char text[80];pc_settings_key_name(keys[k],text,sizeof text);[b addItemWithTitle:[NSString stringWithUTF8String:text]];b.lastItem.representedObject=@(keys[k]);}
            b.toolTip=[NSString stringWithFormat:@"%s — binding %d",pc_action_labels[a],slot+1];
            [rows addSubview:b];
            NSButton *record=[self button:@"Record" action:@selector(record:) frame:NSMakeRect(278+slot*146,y,57,28) view:rows];
            record.tag=a*PC_BINDING_SLOTS+slot;
            [record setAccessibilityLabel:[NSString stringWithFormat:@"Record %s binding %d",pc_action_labels[a],slot+1]];
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
    label(gv,@"Aspect ratio",NSMakeRect(25,470,180,25));
    aspect=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(220,467,340,30) pullsDown:NO];[gv addSubview:aspect];
    for(int i=0;i<PC_DISPLAY_ASPECT_COUNT;i++)[aspect addItemWithTitle:[NSString stringWithUTF8String:pc_display_aspect_name(i)]];
    aspect.target=self;aspect.action=@selector(aspectChanged:);[aspect setAccessibilityLabel:@"Aspect ratio"];
    label(gv,@"Render resolution",NSMakeRect(25,420,180,25));
    resolution=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(220,417,340,30) pullsDown:NO];[gv addSubview:resolution];
    [resolution setAccessibilityLabel:@"Render resolution"];
    all_mode_count=pc_display_modes(game_window,all_modes,PC_DISPLAY_MODE_CAPACITY);
    PcDisplayMode current=pc_display_current();
    [aspect selectItemAtIndex:0];[self fillModes:current];
    label(gv,@"Vertical field of view",NSMakeRect(25,365,180,25));
    fov=[NSSlider sliderWithValue:pc_display_fov() minValue:PC_FOV_MIN maxValue:PC_FOV_MAX target:self action:@selector(fovChanged:)];
    fov.frame=NSMakeRect(220,365,300,25);fov.continuous=YES;[fov setAccessibilityLabel:@"Vertical field of view"];[gv addSubview:fov];
    fov_value=label(gv,@"",NSMakeRect(535,365,80,25));[self fovChanged:nil];
    fullscreen=[NSButton checkboxWithTitle:@"Fullscreen" target:nil action:NULL];fullscreen.frame=NSMakeRect(25,310,300,30);fullscreen.state=!!(SDL_GetWindowFlags(game_window)&SDL_WINDOW_FULLSCREEN);[gv addSubview:fullscreen];
    label(gv,@"FOV changes gameplay view and keeps weapon zoom relative. Default: 55°.\nHigher resolutions increase GPU load; the game clock stays at 60 Hz.\n\nUse the game's Display settings entry for a timed preview.\nSelect texture packs through the launcher’s --texture-pack option.",NSMakeRect(25,100,740,160));
    [self button:@"Defaults" action:@selector(defaults:) frame:NSMakeRect(20,15,110,32) view:view];
    [self button:@"Cancel" action:@selector(cancel:) frame:NSMakeRect(610,15,100,32) view:view];
    NSButton *save=[self button:@"Save this tab" action:@selector(save:) frame:NSMakeRect(720,15,120,32) view:view];save.keyEquivalent=@"\r";
    [panel center];[panel makeKeyAndOrderFront:nil];
    NSTimer *testTimer=nil;
    if(graphics_test_ready){testTimer=[NSTimer timerWithTimeInterval:.05 repeats:NO block:^(NSTimer *timer){graphics_test_ready(self,tabs,aspect,resolution,fov);}];
        [[NSRunLoop mainRunLoop] addTimer:testTimer forMode:NSModalPanelRunLoopMode];}
    [NSApp runModalForWindow:panel];[testTimer invalidate];[panel orderOut:nil];panel=nil;
    SDL_RaiseWindow(game_window);
    tsfp_native_settings_release_input(game_window);
}
- (void)fillModes:(PcDisplayMode)staged {
    int selected=0;
    mode_count=pc_display_filter_modes(all_modes,all_mode_count,(int)aspect.indexOfSelectedItem,
        staged,modes,PC_DISPLAY_MODE_CAPACITY,&selected);
    [resolution removeAllItems];
    for(int i=0;i<mode_count;i++)[resolution addItemWithTitle:pc_display_is_auto(modes[i])?@"Auto (current display)":modes[i].width?
        [NSString stringWithFormat:@"%u × %u",modes[i].width,modes[i].height]:@"Original (640 × 480)"];
    if(mode_count)[resolution selectItemAtIndex:selected];
}
- (void)aspectChanged:(id)sender {
    NSInteger index=resolution.indexOfSelectedItem;
    [self fillModes:index>=0 && index<mode_count?modes[index]:pc_display_current()];
}
- (void)fovChanged:(id)sender {
    fov.intValue=(int)lround(fov.doubleValue);
    fov_value.stringValue=[NSString stringWithFormat:@"%d°",fov.intValue];
}
- (PcSettings)bindingValues {
    PcSettings next=original;
    for(int a=0;a<PC_ACTION_COUNT;a++)for(int slot=0;slot<PC_BINDING_SLOTS;slot++)
        next.keys[a][slot]=[bindings[a][slot].selectedItem.representedObject intValue];
    return next;
}
- (void)record:(id)sender {
    int action=(int)[sender tag]/PC_BINDING_SLOTS,slot=(int)[sender tag]%PC_BINDING_SLOTS;
    if(action<0 || action>=PC_ACTION_COUNT)return;
    int key=capture_binding([NSString stringWithUTF8String:pc_action_labels[action]],nil);
    if(key<0)return;
    PcSettings next=[self bindingValues];int conflict=pc_settings_conflict(&next,action,key);
    if(conflict>=0) {
        char name[80];pc_settings_key_name(key,name,sizeof name);
        NSAlert *confirm=[NSAlert new];confirm.messageText=@"Binding already in use";
        confirm.informativeText=[NSString stringWithFormat:@"%s is assigned to %s. Move it to %s? The change applies only when you save.",name,pc_action_labels[conflict],pc_action_labels[action]];
        [confirm addButtonWithTitle:@"Cancel"];[confirm addButtonWithTitle:@"Move binding"];
        if([confirm runModal]!=NSAlertSecondButtonReturn)return;
    }
    if(pc_settings_assign(&next,action,slot,key,1)==1)[self fillBindings:&next];
}
- (void)save:(id)sender {
    char error[512];
    if([tabs.selectedTabViewItem.identifier isEqual:@"controls"]) {
        PcSettings next=[self bindingValues];
        for(int a=0;a<PC_ACTION_COUNT;a++)for(int slot=0;slot<PC_BINDING_SLOTS;slot++) {
            int conflict=pc_settings_conflict(&next,a,next.keys[a][slot]);
            if(conflict>=0){snprintf(error,sizeof error,"%s and %s share a binding. Set one to Unbound before saving.",pc_action_labels[a],pc_action_labels[conflict]);alert_text(error);return;}
        }
        next.sensitivity=sensitivity.doubleValue;next.enabled=enabled.state==NSControlStateValueOn;next.invert_y=invert.state==NSControlStateValueOn;
        if(!pc_settings_save(&next,error,sizeof error)){alert_text(error);return;}
    } else {
        NSInteger index=resolution.indexOfSelectedItem;
        if(index<0 || index>=mode_count){alert_text("Select an available render resolution.");return;}
        PcDisplayMode before=pc_display_current(),next=modes[index];int before_fov=pc_display_fov();
        if(!pc_display_apply(next)){alert_text("The selected render resolution could not be allocated. Your previous resolution is unchanged.");return;}
        if(!pc_display_set_fov(fov.intValue) || !pc_display_save()){
            pc_display_apply(before);pc_display_set_fov(before_fov);
            alert_text("Could not save the display settings. The previous resolution and FOV were restored.");return;
        }
        if(!SDL_SetWindowFullscreen(game_window,fullscreen.state==NSControlStateValueOn))alert_text(SDL_GetError());
    }
    [NSApp stopModal];
}
- (void)cancel:(id)sender {[NSApp stopModal];}
- (BOOL)windowShouldClose:(NSWindow *)sender {[NSApp stopModal];return NO;}
- (void)defaults:(id)sender {
    if([tabs.selectedTabViewItem.identifier isEqual:@"controls"]){PcSettings value;pc_settings_defaults(&value);[self fill:&value];}
    else {[aspect selectItemAtIndex:0];[self fillModes:(PcDisplayMode){0,0}];fov.intValue=PC_FOV_DEFAULT;[self fovChanged:nil];fullscreen.state=NSControlStateValueOff;}
}
@end
#include "graphics_settings_selftest.inc"
@interface TSFPMenu : NSObject
- (void)settings:(id)sender;
- (void)fullscreen:(id)sender;
- (void)debug:(id)sender;
@end
@implementation TSFPMenu
- (void)debug:(id)sender {pc_debug_show(NULL);}
- (void)settings:(id)sender {tsfp_native_settings_show(game_window);}
- (void)fullscreen:(id)sender {tsfp_native_settings_release_input(game_window);SDL_SetWindowFullscreen(game_window,!(SDL_GetWindowFlags(game_window)&SDL_WINDOW_FULLSCREEN));}
@end
static TSFPMenu *menu_target;
void tsfp_native_settings_attach(SDL_Window *window) {
    game_window=window;if(menu_target)return;menu_target=[TSFPMenu new];
    native_debug_attach();
    NSMenu *menu=[[NSMenu alloc] initWithTitle:@"Game Settings"];
    NSMenuItem *settings_item=[[NSMenuItem alloc] initWithTitle:@"Keyboard, Mouse & Graphics…" action:@selector(settings:) keyEquivalent:@","];
    settings_item.target=menu_target;[menu addItem:settings_item];
    NSMenuItem *full=[[NSMenuItem alloc] initWithTitle:@"Toggle Fullscreen" action:@selector(fullscreen:) keyEquivalent:@"f"];
    full.keyEquivalentModifierMask=NSEventModifierFlagCommand|NSEventModifierFlagControl;full.target=menu_target;[menu addItem:full];
    NSMenuItem *debug=[[NSMenuItem alloc] initWithTitle:@"Debug & Performance…" action:@selector(debug:) keyEquivalent:@""];
    debug.target=menu_target;[menu addItem:debug];
    NSMenuItem *top=[[NSMenuItem alloc] initWithTitle:@"Game Settings" action:NULL keyEquivalent:@""];top.submenu=menu;[NSApp.mainMenu addItem:top];
}
void tsfp_native_settings_show(SDL_Window *window) {
    game_window=window;if(!settings)settings=[TSFPSettings new];[settings show];
}
