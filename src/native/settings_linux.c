/* Linux desktop boundary. Configuration, bindings and display policy stay in
 * the shared models; GTK widgets only stage values and invoke those models. */
#include "settings_macos.h"
#include "input_sdl.h"
#include "pc_settings.h"
#include "pc_display.h"
#include "pc_debug.h"
#include "pc_storage.h"
#include "crash_posix.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static SDL_Window *game_window;
static atomic_int picker_state;
static char picker_path[4096];
static void selected(void *context,const char *const *files,int filter){
    (void)context;(void)filter;
    int accepted=files&&files[0]&&strlen(files[0])<sizeof picker_path;
    if(accepted)snprintf(picker_path,sizeof picker_path,"%s",files[0]);
    atomic_store(&picker_state,accepted?2:3);
}
int pc_storage_picker_start(void){
    int expected=0;if(!atomic_compare_exchange_strong(&picker_state,&expected,1))return 0;
    SDL_ShowOpenFolderDialog(selected,NULL,game_window,NULL,false);return 1;
}
int pc_storage_picker_poll(char *out,size_t capacity){
    int state=atomic_load(&picker_state);if(state<2)return 0;
    if(state==2)snprintf(out,capacity,"%s",picker_path);
    atomic_store(&picker_state,0);return state==2?1:-1;
}
#include <gtk/gtk.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <limits.h>
static int dialog_test; /* Nonzero only from isolated native self-tests. */
static int gtk_ready(void){return gtk_init_check(NULL,NULL);}
static void alert(GtkWidget *parent,const char *text){
    if(dialog_test){fprintf(stderr,"[linux-dialog-test] %s\n",text);return;}
    GtkWidget *m=gtk_message_dialog_new(GTK_WINDOW(parent),GTK_DIALOG_MODAL,
        GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,"%s",text);
    gtk_dialog_run(GTK_DIALOG(m));gtk_widget_destroy(m);
}
static GtkWidget *row(GtkWidget *grid,int n,const char *name,GtkWidget *input){
    GtkWidget *label=gtk_label_new(name);gtk_widget_set_halign(label,GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid),label,0,n,1,1);gtk_grid_attach(GTK_GRID(grid),input,1,n,1,1);
    return input;
}
static GtkWidget *grid(void){
    GtkWidget *g=gtk_grid_new();gtk_grid_set_row_spacing(GTK_GRID(g),8);
    gtk_grid_set_column_spacing(GTK_GRID(g),12);gtk_container_set_border_width(GTK_CONTAINER(g),12);return g;
}
static GtkWidget *check(const char *name,int on){
    GtkWidget *b=gtk_check_button_new_with_label(name);gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b),on);return b;
}
static int checked(GtkWidget *w){return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));}
static GtkWidget *dialog(const char *title){
    GtkWidget *d=gtk_dialog_new_with_buttons(title,NULL,GTK_DIALOG_MODAL,
        "Cancel",GTK_RESPONSE_CANCEL,"Save & Close",GTK_RESPONSE_ACCEPT,NULL);
    gtk_window_set_default_size(GTK_WINDOW(d),760,620);return d;
}
typedef struct {
    GtkWidget *dialog,*tabs,*keys[PC_ACTION_COUNT][PC_BINDING_SLOTS];
    GtkWidget *sensitivity,*enabled,*invert,*resolution,*fov,*fullscreen;
    PcDisplayMode modes[PC_DISPLAY_MODE_CAPACITY];int mode_count;
} LinuxSettings;
static void fill_controls(LinuxSettings *e,const PcSettings *value){
    for(int a=0;a<PC_ACTION_COUNT;a++)for(int slot=0;slot<PC_BINDING_SLOTS;slot++){
        char id[16];snprintf(id,sizeof id,"%d",value->keys[a][slot]);
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(e->keys[a][slot]),id);
    }
    char text[32];snprintf(text,sizeof text,"%.6g",value->sensitivity);
    gtk_entry_set_text(GTK_ENTRY(e->sensitivity),text);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->enabled),value->enabled);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->invert),value->invert_y);
}
static void defaults(GtkButton *button,gpointer data){
    (void)button;LinuxSettings *e=data;
    if(gtk_notebook_get_current_page(GTK_NOTEBOOK(e->tabs))==0){PcSettings value;pc_settings_defaults(&value);fill_controls(e,&value);}
    else {gtk_combo_box_set_active(GTK_COMBO_BOX(e->resolution),0);gtk_range_set_value(GTK_RANGE(e->fov),PC_FOV_DEFAULT);gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->fullscreen),0);}
}
static LinuxSettings make_settings(const PcSettings *initial){
    LinuxSettings e={0};e.dialog=dialog("TimeSplitters Settings");e.tabs=gtk_notebook_new();
    GtkWidget *body=gtk_dialog_get_content_area(GTK_DIALOG(e.dialog));gtk_box_pack_start(GTK_BOX(body),e.tabs,TRUE,TRUE,0);
    GtkWidget *controls=gtk_box_new(GTK_ORIENTATION_VERTICAL,8),*g=grid();
    GtkWidget *scroll=gtk_scrolled_window_new(NULL,NULL);gtk_widget_set_vexpand(scroll,TRUE);
    gtk_container_add(GTK_CONTAINER(scroll),g);gtk_box_pack_start(GTK_BOX(controls),scroll,TRUE,TRUE,0);
    for(int a=0;a<PC_ACTION_COUNT;a++){
        gtk_grid_attach(GTK_GRID(g),gtk_label_new(pc_action_labels[a]),0,a,1,1);
        for(int slot=0;slot<PC_BINDING_SLOTS;slot++){
            GtkWidget *combo=gtk_combo_box_text_new();e.keys[a][slot]=combo;
            for(int k=0;k<256;k++)if(!k||pc_settings_key_allowed(k)||k==initial->keys[a][slot]){
                char id[16],name[80];snprintf(id,sizeof id,"%d",k);pc_settings_key_name(k,name,sizeof name);
                gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo),id,name);
            }
            gtk_widget_set_tooltip_text(combo,pc_action_labels[a]);gtk_grid_attach(GTK_GRID(g),combo,1+slot,a,1,1);
        }
    }
    GtkWidget *mouse=grid();gtk_box_pack_start(GTK_BOX(controls),mouse,FALSE,FALSE,0);
    e.sensitivity=row(mouse,0,"Mouse sensitivity (0.001Ã¢â‚¬â€œ10)",gtk_entry_new());
    e.enabled=check("Enable mouse look",0);e.invert=check("Invert mouse Y",0);
    gtk_grid_attach(GTK_GRID(mouse),e.enabled,0,1,2,1);gtk_grid_attach(GTK_GRID(mouse),e.invert,0,2,2,1);
    gtk_notebook_append_page(GTK_NOTEBOOK(e.tabs),controls,gtk_label_new("Keyboard & Mouse"));
    GtkWidget *graphics=grid();e.resolution=gtk_combo_box_text_new();
    e.mode_count=pc_display_modes((HWND)game_window,e.modes,PC_DISPLAY_MODE_CAPACITY);
    PcDisplayMode current=pc_display_current();int selected=0;
    for(int i=0;i<e.mode_count;i++){
        char name[80];PcDisplayMode m=e.modes[i];
        if(pc_display_is_auto(m))snprintf(name,sizeof name,"Auto (window size)");
        else if(!m.width&&!m.height)snprintf(name,sizeof name,"Original (640 Ãƒâ€” 480)");
        else snprintf(name,sizeof name,"%u Ãƒâ€” %u",m.width,m.height);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(e.resolution),name);
        if(m.width==current.width&&m.height==current.height)selected=i;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(e.resolution),selected);row(graphics,0,"Render resolution",e.resolution);
    e.fov=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,PC_FOV_MIN,PC_FOV_MAX,1);
    gtk_range_set_value(GTK_RANGE(e.fov),pc_display_fov());row(graphics,1,"Field of view",e.fov);
    e.fullscreen=check("Fullscreen",game_window&&(SDL_GetWindowFlags(game_window)&SDL_WINDOW_FULLSCREEN));
    gtk_grid_attach(GTK_GRID(graphics),e.fullscreen,0,2,2,1);
    gtk_notebook_append_page(GTK_NOTEBOOK(e.tabs),graphics,gtk_label_new("Graphics"));fill_controls(&e,initial);
    return e;
}
static int save_controls(LinuxSettings *e,char *error,size_t bytes){
    PcSettings next={0};
    for(int a=0;a<PC_ACTION_COUNT;a++)for(int slot=0;slot<PC_BINDING_SLOTS;slot++){
        const char *id=gtk_combo_box_get_active_id(GTK_COMBO_BOX(e->keys[a][slot]));
        if(!id){snprintf(error,bytes,"Select a binding for every action.");return 0;}next.keys[a][slot]=atoi(id);
    }
    for(int a=0;a<PC_ACTION_COUNT;a++)for(int slot=0;slot<PC_BINDING_SLOTS;slot++){
        int other=pc_settings_conflict(&next,a,next.keys[a][slot]);
        if(other>=0){snprintf(error,bytes,"%s and %s share a binding. Unbind one before saving.",pc_action_labels[a],pc_action_labels[other]);return 0;}
    }
    const char *text=gtk_entry_get_text(GTK_ENTRY(e->sensitivity));char *end;
    next.sensitivity=g_ascii_strtod(text,&end);
    if(end==text||*end||!isfinite(next.sensitivity)){snprintf(error,bytes,"Enter a valid mouse sensitivity.");return 0;}
    next.enabled=checked(e->enabled);next.invert_y=checked(e->invert);
    return pc_settings_save(&next,error,bytes);
}
void tsfp_native_settings_attach(SDL_Window *window){game_window=window;pc_debug_init((HWND)window);}
void tsfp_native_settings_show(SDL_Window *window){
    game_window=window;if(!gtk_ready()){SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Settings","Could not connect to the desktop display.",window);return;}
    PcSettings initial;char error[512];if(!pc_settings_read(&initial,error,sizeof error))return;
    tsfp_native_settings_release_input(window);tsfp_native_crash_inhibit(1);
    LinuxSettings e=make_settings(&initial);GtkWidget *button=gtk_dialog_add_button(GTK_DIALOG(e.dialog),"Defaults",100);
    g_signal_connect(button,"clicked",G_CALLBACK(defaults),&e);gtk_widget_show_all(e.dialog);
    for(;;){int response=gtk_dialog_run(GTK_DIALOG(e.dialog));if(response==100)continue;if(response!=GTK_RESPONSE_ACCEPT)break;
        if(gtk_notebook_get_current_page(GTK_NOTEBOOK(e.tabs))==0){if(!save_controls(&e,error,sizeof error)){alert(e.dialog,error);continue;}}
        else {
            int i=gtk_combo_box_get_active(GTK_COMBO_BOX(e.resolution));PcDisplayMode before=pc_display_current();int old_fov=pc_display_fov();
            if(i<0||i>=e.mode_count||!pc_display_apply(e.modes[i])){alert(e.dialog,"The selected resolution could not be applied.");continue;}
            if(!pc_display_set_fov((int)gtk_range_get_value(GTK_RANGE(e.fov)))||!pc_display_save()){
                pc_display_apply(before);pc_display_set_fov(old_fov);alert(e.dialog,"Could not save graphics settings. Previous values were restored.");continue;
            }
            if(window&&!SDL_SetWindowFullscreen(window,checked(e.fullscreen))){alert(e.dialog,SDL_GetError());continue;}
        }break;
    }
    gtk_widget_destroy(e.dialog);tsfp_native_settings_release_input(window);tsfp_native_crash_inhibit(0);
}
int pc_debug_open_logs(HWND owner){
    (void)owner;const char *path=getenv("RECOMP_LOG_FILE");if(!path||!*path)return 0;
    char *directory=g_path_get_dirname(path);char *uri=g_filename_to_uri(directory,NULL,NULL);g_free(directory);
    int ok=uri&&gtk_show_uri_on_window(NULL,uri,GDK_CURRENT_TIME,NULL);g_free(uri);return ok;
}
typedef struct {GtkWidget *dialog,*summary,*log,*checks[PC_DEBUG_OPTION_COUNT];} LinuxDebug;
static gboolean refresh_debug(gpointer data){
    LinuxDebug *e=data;pc_debug_poll();PcDebugSnapshot s;pc_debug_snapshot(&s);
    char text[256];snprintf(text,sizeof text,"Game: %.1f FPS   Shown: %.1f FPS   Frame: %.2f ms\nSlow frames: %lu   Errors: %lu   Warnings: %lu",s.fps,s.shown_fps,s.frame_ms,s.slow_frames,s.errors,s.warnings);
    gtk_label_set_text(GTK_LABEL(e->summary),text);GtkTextBuffer *b=gtk_text_view_get_buffer(GTK_TEXT_VIEW(e->log));
    char *valid=g_utf8_make_valid(s.log_available?s.recent:"No log available for this session.",-1);gtk_text_buffer_set_text(b,valid,-1);g_free(valid);return G_SOURCE_CONTINUE;
}
static void debug_logs(GtkButton *b,gpointer data){(void)b;pc_debug_open_logs(data);}
static void debug_clear(GtkButton *b,gpointer data){(void)b;(void)data;pc_debug_clear();}
static gboolean debug_test_response(gpointer data){
    LinuxDebug *e=data;
    for(unsigned i=0;i<PC_DEBUG_OPTION_COUNT;i++)if(e->checks[i])gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->checks[i]),!checked(e->checks[i]));
    gtk_dialog_response(GTK_DIALOG(e->dialog),dialog_test==2?GTK_RESPONSE_CANCEL:GTK_RESPONSE_ACCEPT);return G_SOURCE_REMOVE;
}
void pc_debug_show(HWND owner){
    if(!gtk_ready())return;tsfp_native_settings_release_input(game_window);tsfp_native_crash_inhibit(1);
    LinuxDebug e={0};e.dialog=dialog("Debug & Performance");GtkWidget *body=gtk_dialog_get_content_area(GTK_DIALOG(e.dialog));
    e.summary=gtk_label_new("");gtk_box_pack_start(GTK_BOX(body),e.summary,FALSE,FALSE,10);
    PcDebugOptions original;pc_debug_get(&original);
    for(unsigned i=0;i<PC_DEBUG_OPTION_COUNT;i++)if(pc_debug_option_available(i,0)){
        e.checks[i]=check(pc_debug_option(i)->label,pc_debug_option_value(&original,i));gtk_box_pack_start(GTK_BOX(body),e.checks[i],FALSE,FALSE,4);
    }
    GtkWidget *scroll=gtk_scrolled_window_new(NULL,NULL);e.log=gtk_text_view_new();gtk_text_view_set_editable(GTK_TEXT_VIEW(e.log),FALSE);
    gtk_container_add(GTK_CONTAINER(scroll),e.log);gtk_box_pack_start(GTK_BOX(body),scroll,TRUE,TRUE,8);
    GtkWidget *buttons=gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL),*logs=gtk_button_new_with_label("Open logs folder"),*clear=gtk_button_new_with_label("Clear counters");
    gtk_container_add(GTK_CONTAINER(buttons),logs);gtk_container_add(GTK_CONTAINER(buttons),clear);gtk_box_pack_start(GTK_BOX(body),buttons,FALSE,FALSE,8);
    g_signal_connect(logs,"clicked",G_CALLBACK(debug_logs),owner);g_signal_connect(clear,"clicked",G_CALLBACK(debug_clear),NULL);
    refresh_debug(&e);guint timer=g_timeout_add(500,refresh_debug,&e);gtk_widget_show_all(e.dialog);
    if(dialog_test)g_timeout_add(30,debug_test_response,&e);
    while(gtk_dialog_run(GTK_DIALOG(e.dialog))==GTK_RESPONSE_ACCEPT){
        PcDebugOptions next=original;for(unsigned i=0;i<PC_DEBUG_OPTION_COUNT;i++)if(e.checks[i])pc_debug_option_set(&next,i,checked(e.checks[i]));
        char error[512];if(!pc_debug_config_save(&next,error,sizeof error)){alert(e.dialog,error);continue;}pc_debug_set(&next);break;
    }
    g_source_remove(timer);gtk_widget_destroy(e.dialog);tsfp_native_settings_release_input(game_window);tsfp_native_crash_inhibit(0);
}
int tsfp_native_settings_test(void){
    if(!getenv("RECOMP_INPUT_CONFIG")||!gtk_ready())return 1;
    PcSettings initial,actual;char error[512];if(!pc_settings_read(&initial,error,sizeof error))return 0;
    dialog_test=1;LinuxSettings e=make_settings(&initial);gtk_widget_show_all(e.dialog);
    gtk_entry_set_text(GTK_ENTRY(e.sensitivity),"nan");int ok=!save_controls(&e,error,sizeof error);
    gtk_entry_set_text(GTK_ENTRY(e.sensitivity),"0.25");ok&=save_controls(&e,error,sizeof error);
    ok&=pc_settings_read(&actual,error,sizeof error)&&fabs(actual.sensitivity-.25)<1e-9;
    fill_controls(&e,&initial);ok&=save_controls(&e,error,sizeof error);gtk_widget_destroy(e.dialog);dialog_test=0;
    fprintf(stderr,"%s: GTK controls, invalid number rejection, save/reload and restored values\n",ok?"PASS":"FAIL");return ok?0:1;
}
int tsfp_native_debug_test(void){
    if(!getenv("RECOMP_INPUT_CONFIG")||!gtk_ready())return 1;
    PcDebugOptions old,read;char error[512];pc_debug_config_read(&old);pc_debug_set(&old);
    dialog_test=2;pc_debug_show(NULL);pc_debug_config_read(&read);int ok=1;
    for(unsigned i=0;i<PC_DEBUG_OPTION_COUNT;i++)ok&=pc_debug_option_value(&read,i)==pc_debug_option_value(&old,i);
    dialog_test=3;pc_debug_show(NULL);pc_debug_config_read(&read);dialog_test=0;
    for(unsigned i=0;i<PC_DEBUG_OPTION_COUNT;i++){
        int expected=pc_debug_option_value(&old,i);if(pc_debug_option_available(i,0))expected=!expected;
        ok&=pc_debug_option_value(&read,i)==expected;
    }
    ok&=pc_debug_config_save(&old,error,sizeof error);pc_debug_set(&old);
    fprintf(stderr,"%s: real GTK debug Save/Cancel, shared definitions and unsupported-value retention\n",ok?"PASS":"FAIL");return ok?0:1;
}
int tsfp_native_capture_test(void){return 77;}
int tsfp_native_graphics_settings_test(SDL_Window *window){
    (void)window;PcDisplayMode old=pc_display_current();int ok=pc_display_apply((PcDisplayMode){640,480});
    PcDisplayMode actual=pc_display_current();ok &= actual.width==640&&actual.height==480;ok &= pc_display_apply(old);return ok;
}
