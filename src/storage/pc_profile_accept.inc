/* US 45410066 frontend Controls Accept (also the save-changes Yes path).
 * sub_002CBD20 commits the draft and marks profiles dirty, but the console
 * only writes them when leaving Settings. Run its existing signed-save and
 * error-dialog workflow now, returning to Settings on success. Cancel never
 * calls this hook. Gameplay/pause controls retain their original behavior. */
extern void sub_001922D0(void);
void pc_profile_accept(void (*original)(void))
{
    original();
    if ((int32_t)WORD_AT(0x0079094Cu)<=100 ||
        !(WORD_AT(0x0078AD24u)&0x10000u) ||
        (WORD_AT(0x0078AD24u)&1u)) return;
    uint32_t profile=WORD_AT(0x0078AD34u),menu=WORD_AT(0x007613E4u);
    if (profile>=WORD_AT(0x0074E934u) || !guest_writable(menu,0x170)) return;
    /* Use the same all-profile pass as leaving Settings. The dirty flag is
     * global: a selected-profile-only save would clear other pending commits.
     * Unaccepted edits remain in the menu draft, outside these profiles. */
    guest_call(sub_001922D0,3,menu,0x00523910u,UINT32_MAX);
}
