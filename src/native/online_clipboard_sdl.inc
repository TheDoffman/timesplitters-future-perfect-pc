/* SDL clipboard APIs belong to the main thread. Queue without waiting for
 * the renderer, which may itself be waiting for this guest menu frame. */
#include <pthread.h>
static pthread_mutex_t online_clipboard_lock=PTHREAD_MUTEX_INITIALIZER;
static char online_clipboard_text[2048];
static int online_clipboard_state,online_clipboard_result;
static void SDLCALL online_clipboard_main(void *unused)
{
    (void)unused;pthread_mutex_lock(&online_clipboard_lock);
    if(online_clipboard_state==1){SDL_SetClipboardText(online_clipboard_text);online_clipboard_state=0;}
    else if(online_clipboard_state==2){
        char *text=SDL_GetClipboardText();
        online_clipboard_result=text && *text && strlen(text)<sizeof online_clipboard_text;
        if(online_clipboard_result)memcpy(online_clipboard_text,text,strlen(text)+1);
        SDL_free(text);online_clipboard_state=3;
    }
    pthread_mutex_unlock(&online_clipboard_lock);
}
int pc_online_menu_copy(const char *text)
{
    if(!text || strlen(text)>=sizeof online_clipboard_text)return 0;
    pthread_mutex_lock(&online_clipboard_lock);
    if(online_clipboard_state){pthread_mutex_unlock(&online_clipboard_lock);return 0;}
    memcpy(online_clipboard_text,text,strlen(text)+1);online_clipboard_state=1;
    pthread_mutex_unlock(&online_clipboard_lock);
    if(SDL_RunOnMainThread(online_clipboard_main,NULL,false))return 1;
    pthread_mutex_lock(&online_clipboard_lock);online_clipboard_state=0;pthread_mutex_unlock(&online_clipboard_lock);return 0;
}
int pc_online_menu_paste(char *text,size_t capacity)
{
    pthread_mutex_lock(&online_clipboard_lock);
    if(online_clipboard_state==3){
        int ok=online_clipboard_result && strlen(online_clipboard_text)<capacity;
        if(ok)memcpy(text,online_clipboard_text,strlen(online_clipboard_text)+1);
        online_clipboard_state=0;pthread_mutex_unlock(&online_clipboard_lock);return ok;
    }
    if(online_clipboard_state){pthread_mutex_unlock(&online_clipboard_lock);return -1;}
    online_clipboard_state=2;pthread_mutex_unlock(&online_clipboard_lock);
    if(SDL_RunOnMainThread(online_clipboard_main,NULL,false))return -1;
    pthread_mutex_lock(&online_clipboard_lock);online_clipboard_state=0;pthread_mutex_unlock(&online_clipboard_lock);return 0;
}
