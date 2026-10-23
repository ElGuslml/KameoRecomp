// The launcher's texts, UTF-8, in the Windows display language: English, German, French,
// Spanish and Italian (the Spanish ones are the mockup's). The game asks for its own language in its
// first menu.

#pragma once

#include <cstdint>

namespace launcher {

struct Texts {
  // The sidebar.
  const char* brand;  // under the game's name
  const char* nav_play;
  const char* nav_achievements;
  const char* nav_controls;
  const char* nav_keyboard;
  const char* nav_gamepad;
  const char* nav_settings;
  const char* nav_video;
  const char* nav_graphics;
  const char* nav_game;
  const char* nav_updates;
  const char* donate_text;
  const char* donate;
  const char* version;  // %s: the version
  // The controller's hints, under the sidebar.
  const char* hint_section;
  const char* hint_select;
  const char* hint_back;
  const char* hint_play;

  // Shared by the pages.
  const char* autosave;
  const char* reset_values;
  const char* reset_defaults;
  const char* coming_soon;  // a badge, upper case
  const char* settings;     // the kicker of the Settings pages
  const char* controls;     // the kicker of the Controls pages

  // Play.
  const char* tagline;
  const char* quit;
  const char* play;  // upper case
  const char* current_settings;
  const char* change_settings;
  const char* achievements_card;
  const char* see_achievements;
  const char* achievements_after_play;
  const char* updates_card;
  const char* find_updates;
  const char* last_check;  // %s: when
  const char* never;

  // Video.
  const char* video_subtitle;
  const char* monitor;
  const char* monitor_hint;
  const char* primary;
  const char* display;
  const char* display_hint;
  const char* fullscreen;
  const char* borderless;
  const char* window;
  const char* resolution;
  const char* resolution_hint_screen;
  const char* resolution_hint_window;
  const char* monitor_resolution;
  const char* format;
  const char* format_hint;
  const char* format_hint_16x9;
  const char* format_hint_xenos;
  const char* format_fill;
  const char* format_bars;
  const char* internal;
  const char* internal_hint;
  const char* internal_hint_xenos;  // %d × %d: the game's frame
  const char* automatic;
  const char* original;

  // Graphics.
  const char* graphics_api;
  const char* recommended;  // a badge, upper case
  const char* native_text;
  const char* native_missing;
  const char* xenos_text;
  const char* framerate;
  const char* framerate_hint;
  const char* antialiasing;
  const char* antialiasing_hint;
  const char* antialiasing_hint_xenos;
  const char* none;
  const char* experimental;
  const char* anisotropic;
  const char* anisotropic_hint;
  const char* off;
  const char* shadows;
  const char* shadows_hint;
  const char* shadows_high;
  const char* shadows_ultra;
  const char* api_hint;

  // Keyboard.
  const char* keyboard_subtitle;
  const char* mnk;
  const char* mnk_hint;
  const char* column_control;
  const char* column_primary;
  const char* column_secondary;
  const char* group_movement;
  const char* group_camera;
  const char* group_buttons;
  const char* group_dpad;
  const char* group_mouse;
  const char* binds[24];  // in launcher::Bind order
  const char* unbound;
  const char* press_key;
  const char* modifier_alone;
  const char* mouse_camera;
  const char* mouse_camera_hint;
  const char* mouse_sensitivity;
  const char* mouse_sensitivity_hint;
  const char* key_space;
  const char* key_enter;
  const char* key_backspace;
  const char* key_delete;
  const char* key_insert;
  const char* key_home;
  const char* key_end;
  const char* key_page_up;
  const char* key_page_down;
  const char* key_caps_lock;
  const char* key_num_lock;
  const char* key_scroll_lock;
  const char* key_print_screen;
  const char* key_pause;
  const char* key_numpad;  // the prefix of the numeric keypad's keys
  const char* key_shift;
  const char* key_ctrl;
  const char* key_alt;
  const char* mouse_left;
  const char* mouse_right;
  const char* mouse_middle;

  // Gamepad.
  const char* gamepad_subtitle;
  const char* detected;
  const char* connected;
  const char* find_controllers;
  const char* no_controllers;
  const char* mapping_title;  // upper case, followed by an arrow and the controller
  const char* mapping_hint;
  const char* pad_buttons[14];  // A B X Y LB RB LT RT LS RS Start Back D-pad Guide
  const char* press_button;
  const char* remap_note;
  const char* controller;  // a controller of unknown type

  // Achievements.
  const char* achievements_title;
  const char* achievements_subtitle;
  const char* filter_all;       // %d
  const char* filter_unlocked;  // %d
  const char* filter_locked;    // %d
  const char* unlocked_total;   // upper case
  const char* gamerscore;       // upper case
  const char* progress;
  const char* unlocked;
  const char* locked;
  const char* achievements_empty_title;
  const char* achievements_empty;
  const char* achievement_secret;       // a secret achievement's name while locked
  const char* achievement_secret_hint;  // and its description
  const char* achievements_filter_empty;

  // Updates.
  const char* updates_title;
  const char* updates_subtitle;
  const char* installed_version;  // upper case
  const char* check_updates;
  const char* checking;
  const char* checking_status;
  const char* up_to_date;
  // The Game page (the game's own options, game.h).
  const char* game_subtitle;
  const char* game_language;
  const char* game_language_hint;
  const char* updates_off;  // the game has no releases to look for (game.h)
  const char* check_on_start;
  const char* check_on_start_hint;
  const char* release_notes;  // upper case; %s: the version
  const char* release_notes_none;
  const char* whats_new;  // %s: the version
  const char* checking_updates;
  const char* check_failed;  // %s: the error
  const char* check_rate_limited;
  const char* update_available;
  const char* update_published;  // %s: date, %s: size in MB
  const char* update_now;
  const char* update_downloading;  // %s of %s MB
  const char* update_verifying;
  const char* update_installing;
  const char* update_restart_hint;
  const char* update_failed;  // %s: the error
  const char* updated_to;  // %s: the version
  const char* update_card_available;  // %s: the version
  const char* see_whats_new;
  const char* cancel;
  const char* update_no_checksum;
  const char* update_checksum_mismatch;
  const char* update_in_use;  // %s: the file
  const char* full_history;

  // The first run (page_install.cpp).
  const char* step_welcome;
  const char* step_iso;
  const char* step_install;
  const char* step_done;
  const char* welcome_title;
  const char* welcome_text;
  const char* need_iso;
  const char* need_iso_hint;  // %s: the game
  const char* install_here;
  const char* install_here_hint;  // %s: the recompiled executable
  const char* need_space;
  const char* need_space_hint;  // %s: GB needed, GB free
  const char* own_copy;
  const char* start;
  const char* choose_title;
  const char* choose_text;  // %s: the game
  const char* drop_here;
  const char* browse;
  const char* checking_iso;
  const char* iso_valid;
  const char* iso_detail;  // %s: the game, the region
  const char* back;
  const char* extract;
  const char* wrong_game_title;
  const char* wrong_game_text;  // %s: the game
  const char* wrong_version_title;
  const char* wrong_version_text;  // %s: the game, the region
  const char* unreadable_title;
  const char* unreadable_text;
  const char* file_label;
  const char* detected_label;
  const char* detected_none;
  const char* detected_no_xex;
  const char* choose_other;
  const char* or_drop;
  const char* no_space;  // %s: GB needed, GB free
  const char* extracting_title;
  const char* extracting_text;
  const char* extracted_title;
  const char* extracted_text;
  const char* time_left;  // %s: mm:ss
  const char* current_file;  // upper case
  const char* iso_verified;  // %s: the game
  const char* cancel_install;
  const char* continue_label;
  const char* extract_failed_title;
  const char* extract_failed_text;  // %s: the file, the system's message
  const char* done_title;
  const char* done_text;
  const char* delete_iso;
  const char* review_settings;
  const char* iso_filter;  // the file dialog's filter
  const char* decimal_separator;  // in sizes (7,3 GB)

  // Errors.
  const char* game_missing;  // %s: the game's executable, %s: the launcher
  const char* starting_game;      // over the launcher until the game's window shows
  const char* game_closed_error;  // %s: the exit code
  const char* open_logs;
  const char* save_failed;  // %s: the options file
  const char* no_direct3d;  // %s: the launcher
};

// The Windows display language as an XLanguage id, like the SDK's user_language: 1 English, 3 German,
// 4 French, 5 Spanish, 6 Italian.
uint32_t SystemLanguage();
const Texts& TextsFor(uint32_t language);

}  // namespace launcher
