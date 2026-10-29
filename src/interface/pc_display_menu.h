#ifndef TSFP_PC_DISPLAY_MENU_H
#define TSFP_PC_DISPLAY_MENU_H
#include <stdint.h>
#define PC_DISPLAY_MENU_CALLBACK 0x00012008u
void pc_display_menu_finish(void (*original)(void));
void pc_display_menu_button(void (*original)(void));
void pc_network_bots_menu(void (*original)(void));
void pc_profile_accept(void (*original)(void));
void pc_unlock_requirement(void (*original)(void));
void pc_unlock_event(void (*original)(void));
void pc_unlock_movie(void (*original)(void));
void pc_unlock_profile_init(void (*original)(void));
uint32_t pc_network_bots_revive(unsigned slot);
void pc_network_bots_entity_refresh(uint32_t entity);
#include "pc_network_bots_state.h"
void pc_network_bots_present(uint32_t entity,const PcNetworkBotState *state);
void pc_network_bots_pose_weapons(uint32_t entity);
void pc_network_bots_present_shot(uint32_t entity,const PcNetworkBotShot *shot);
void pc_network_bots_request_hit(uint32_t context);
int pc_network_bots_kill_notice(unsigned victim,unsigned killer);
void pc_display_menu_mouse(void (*original)(void));
void pc_display_menu_mouse_mode(void);
void pc_display_menu_mouse_input(void (*original)(void));
void pc_display_menu_mouse_footer(void (*original)(void));
void pc_display_menu_mouse_dispatch(void (*original)(void));
void pc_display_menu_audio_draw(void (*original)(void),int compact);
void pc_display_menu_sensitivity_draw(void (*original)(void));
void pc_display_menu_slider_bar(void (*original)(void));
void pc_display_menu_choice_arrow(void (*original)(void),unsigned direction);
void pc_display_menu_character_draw(void (*original)(void));
void pc_display_menu_highlight(void (*original)(void));
void pc_display_menu_choice_draw(void (*original)(void));
void pc_display_menu_team_draw(void (*original)(void));
void pc_display_menu_team_tick(void (*original)(void));
void pc_display_camera(void (*original)(void));
void pc_display_camera_feed(void (*original)(void));
void pc_display_ui(void (*original)(void),uint32_t tag);
void pc_display_scope(void (*original)(void),uint32_t tag);
int pc_display_scope_mask(void (*original)(void));
void pc_display_aim_primitive(void (*original)(void),unsigned kind);
void pc_display_aim_panel(void (*original)(void),uint32_t tag);
void pc_display_effect_scope(void (*original)(void));
void pc_display_weapon_effect(void (*original)(void),unsigned kind);
int pc_display_transition_overlay(void (*original)(void));
void pc_display_flash_overlay(void (*original)(void));
void pc_display_footer_background(void (*original)(void));
void pc_display_title_matte(void (*original)(void));
void pc_display_menu_backdrop(void (*original)(void));
void pc_display_main_menu_font(void (*original)(void),int building);
void pc_display_radar_begin(void);
void pc_display_health(void (*original)(void));
void pc_display_health_side(void);
void pc_display_tutorial(void (*original)(void));
void pc_display_tutorial_wrap(void (*original)(void));
void pc_display_tutorial_text(void (*original)(void));
void pc_display_perspective_projection(void (*original)(void));
void (*pc_display_menu_lookup(uint32_t address))(void);
#endif
