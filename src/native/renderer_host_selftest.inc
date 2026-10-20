/* Exercise the installed production SDL adapter, including its worker boundary. */
typedef struct { SDL_Window *window; uint32_t width, height; int ok; } NativeHostProbe;
static void *native_test_host_worker(void *opaque) {
    NativeHostProbe *probe=opaque;
    probe->ok=rhi_drawable_size(probe->window,&probe->width,&probe->height);
    rhi_pump_events(); /* must not consume main-thread events */
    return NULL;
}
static int native_test_renderer_host(SDL_Window *window, void (*on_quit)(void)) {
    int expected_w=0,expected_h=0;
    uint32_t width=0,height=0;
    int ok=SDL_GetWindowSizeInPixels(window,&expected_w,&expected_h);
    ok &= rhi_drawable_size(window,&width,&height);
    ok &= width==(uint32_t)expected_w && height==(uint32_t)expected_h;
    uint32_t unchanged_w=width,unchanged_h=height;
    ok &= !rhi_drawable_size(NULL,&width,&height);
    ok &= width==unchanged_w && height==unchanged_h;
    Uint32 event_type=SDL_RegisterEvents(1);
    if(!event_type)return 0;
    SDL_Event event={0};event.type=event_type;
    ok &= SDL_PushEvent(&event);
    NativeHostProbe probe={.window=window};pthread_t thread;
    if(pthread_create(&thread,NULL,native_test_host_worker,&probe)!=0)return 0;
    pthread_join(thread,NULL);
    ok &= probe.ok && probe.width==width && probe.height==height;
    ok &= SDL_PeepEvents(&event,1,SDL_PEEKEVENT,event_type,event_type)==1;
    rhi_pump_events();
    ok &= SDL_PeepEvents(&event,1,SDL_PEEKEVENT,event_type,event_type)==0;
    /* Explicitly unregister before destruction; no stale callback/window use. */
    tsfp_renderer_host_bind(NULL,NULL);
    ok &= !rhi_drawable_size(window,&width,&height);
    ok &= width==unchanged_w && height==unchanged_h;
    tsfp_renderer_host_bind(window,on_quit);
    ok &= rhi_drawable_size(window,&width,&height);
    fprintf(stderr,"[native-host-test] physical drawable, worker snapshot, main-thread events, unbind: %s\n",ok?"PASS":"FAIL");
    return ok;
}

/* Exercise OS transitions and the real device present on each target, restoring
 * this disposable test window before returning even when a check fails. */
static int native_test_renderer_window_modes(SDL_Window *window, IDirect3DDevice8 *dev) {
    int old_w=0,old_h=0;
    if(!SDL_GetWindowSize(window,&old_w,&old_h))return 0;
    bool old_fullscreen=(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0;
    int ok=SDL_SetWindowFullscreen(window,false) && SDL_SyncWindow(window);
    ok &= SDL_SetWindowSize(window,640,360) && SDL_SyncWindow(window);
    for(int fullscreen=0;fullscreen<2;fullscreen++) {
        ok &= SDL_SetWindowFullscreen(window,fullscreen!=0) && SDL_SyncWindow(window);
        rhi_pump_events();
        ok &= ((SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0)==(fullscreen!=0);
        int expected_w=0,expected_h=0;uint32_t width=0,height=0;
        ok &= SDL_GetWindowSizeInPixels(window,&expected_w,&expected_h);
        ok &= rhi_drawable_size(window,&width,&height);
        ok &= width==(uint32_t)expected_w && height==(uint32_t)expected_h && width>0 && height>0;
        ok &= SUCCEEDED(dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_TARGET,0xFF00FF00,1,0));
        ok &= dev->lpVtbl->Swap(dev,0)==0;
        fprintf(stderr,"[native-window-test] %s drawable %ux%u, present: %s\n",
            fullscreen?"fullscreen":"windowed",width,height,ok?"PASS":"FAIL");
    }
    ok &= SDL_SetWindowFullscreen(window,false) && SDL_SyncWindow(window);
    ok &= SDL_SetWindowSize(window,old_w,old_h) && SDL_SyncWindow(window);
    if(old_fullscreen)ok &= SDL_SetWindowFullscreen(window,true) && SDL_SyncWindow(window);
    rhi_pump_events();
    int restored_w=0,restored_h=0;
    ok &= SDL_GetWindowSize(window,&restored_w,&restored_h);
    ok &= old_fullscreen || (restored_w==old_w && restored_h==old_h);
    ok &= ((SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0)==old_fullscreen;
    ok &= SUCCEEDED(dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_TARGET,0xFF0000FF,1,0));
    ok &= dev->lpVtbl->Swap(dev,0)==0;
    fprintf(stderr,"[native-window-test] restored window and present: %s\n",ok?"PASS":"FAIL");
    return ok;
}
