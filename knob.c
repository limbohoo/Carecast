#include "font/lv_font.h"
#include "knob.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "event_manager.h"

/* LVGL font externs */
extern const lv_font_t lv_font_montserrat_16;
extern const lv_font_t lv_font_montserrat_12;
extern const lv_font_t lv_font_montserrat_22;
extern const lv_font_t lv_font_montserrat_32;

// --- CONFIG ---

#define SCREEN_SIZE 360
#define EDGE_RADIUS 160       // radius of friend dots from centre
#define SNAP_ANGLE  15        // degrees for snapping
#define TIMEOUT_MS  5000      // 5 s timeout back to ambient

// Eindhoven
#define MY_LAT 51.4416
#define MY_LON 5.4697

// --- STATE TYPES ---

typedef enum {
    PAGE_AMBIENT = 0,
    PAGE_RADAR
} system_page_t;

typedef struct {
    int         id;
    const char *name;       // friend/person label
    double      lat;
    double      lon;
    int         bearing;    // deg 0–359 from Eindhoven
    int         temp;
    lv_obj_t   *ui_dot;
    lv_obj_t   *ui_glow;    // glow circle behind dot
    lv_color_t  base_color; // default dot color
    const char *city;       // city / region label
} friend_t;

// --- GLOBAL STATE ---

static system_page_t current_page    = PAGE_AMBIENT;
static int           radar_angle     = 0;
static int           locked_friend   = -1;
static uint32_t      last_input_time = 0;
static bool          is_sending      = false;

// last known local temperature for reverting bg
int g_my_temp_last = 0;

// two friends: Taiwan & Italy (with distinct colors)
static friend_t friends[2] = {
    {0, "Tina",  24.8138, 120.9675, 0, 0, NULL, NULL,
        LV_COLOR_MAKE(0x40, 0xA0, 0xFF), "Taiwan"},
    {1, "Lucia", 44.1025,   9.8241, 0, 0, NULL, NULL,
        LV_COLOR_MAKE(0xFF, 0x80, 0x40), "Italy"}
};

// --- LVGL OBJECTS ---

static lv_obj_t *scr_main;
static lv_obj_t *layer_ambient;
static lv_obj_t *layer_radar;

// ambient UI
static lv_obj_t *lbl_my_temp;
static lv_obj_t *lbl_my_city;

// radar UI
static lv_obj_t *cursor_ring;
static lv_obj_t *lbl_radar_info;
static lv_obj_t *full_screen_btn;

// --- MATH HELPERS ---

#define DEG_TO_RAD 0.01745329f
#define RAD_TO_DEG 57.2957795f

static int calculate_bearing(double lat1, double lon1,
                             double lat2, double lon2)
{
    double dLon  = (lon2 - lon1) * DEG_TO_RAD;
    double lat1r = lat1 * DEG_TO_RAD;
    double lat2r = lat2 * DEG_TO_RAD;

    double y = sin(dLon) * cos(lat2r);
    double x = cos(lat1r) * sin(lat2r) -
               sin(lat1r) * cos(lat2r) * cos(dLon);

    int bearing = (int)(atan2(y, x) * RAD_TO_DEG);
    return (bearing + 360) % 360;
}

static void get_xy(int angle, int radius, int *x, int *y)
{
    float rad = angle * DEG_TO_RAD;
    *x = (int)(radius * sinf(rad));
    *y = (int)(radius * -cosf(rad));
}

// --- WEATHER COLOR HELPERS (simple, logical) ---

static lv_color_t color_from_temp(int temp)
{
    // Logical temperature color mapping
    if (temp < 0)       return lv_color_hex(0x0D47A1); // deep blue (freezing)
    if (temp < 10)      return lv_color_hex(0x039BE5); // cyan (cold)
    if (temp < 15)      return lv_color_hex(0x43A047); // green (cool)
    if (temp < 20)      return lv_color_hex(0x9CCC65); // lime (mild)
    if (temp < 25)      return lv_color_hex(0xFFA726); // orange (warm)
    if (temp < 30)      return lv_color_hex(0xFF7043); // deep orange (hot)
    return lv_color_hex(0xE53935);                     // red (very hot)
}

static void apply_bg_color(lv_color_t c)
{
    if (layer_ambient) {
        lv_obj_set_style_bg_color(layer_ambient, c, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (layer_radar) {
        lv_obj_set_style_bg_color(layer_radar, c, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void apply_bg_color_from_my_temp(int temp)
{
    lv_color_t c = color_from_temp(temp);
    apply_bg_color(c);
}

static void apply_bg_color_from_friend(int friend_id)
{
    if (friend_id < 0 || friend_id > 1) return;
    int t = friends[friend_id].temp;
    lv_color_t c = color_from_temp(t);
    apply_bg_color(c);
}

// --- PAGE SWITCHING ---

static void set_page(system_page_t page)
{
    current_page    = page;
    last_input_time = lv_tick_get();

    if (page == PAGE_AMBIENT) {
        lv_obj_clear_flag(layer_ambient, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag  (layer_radar,   LV_OBJ_FLAG_HIDDEN);
        // restore home weather color
        apply_bg_color_from_my_temp(g_my_temp_last);
    } else {
        lv_obj_add_flag  (layer_ambient, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(layer_radar,   LV_OBJ_FLAG_HIDDEN);
        
        // CRITICAL: reset text opacity after any previous animation
        lv_obj_set_style_text_opa(lbl_radar_info, LV_OPA_COVER, 0);
        lv_label_set_text(lbl_radar_info, "Friend Search");
    }
}

// --- SEND ANIMATION ---

static void anim_sent_cb(void * var, int32_t v)
{
    lv_obj_set_style_text_opa((lv_obj_t *)var, v, 0);
}

static void on_send_anim_finished(lv_anim_t * a)
{
    LV_UNUSED(a);
    is_sending = false;
    set_page(PAGE_AMBIENT);
}

static void trigger_send(void)
{
    if (locked_friend == -1) return;

    is_sending = true;
    lv_label_set_text(lbl_radar_info, "SENT!");
    lv_obj_set_style_text_opa(lbl_radar_info, LV_OPA_COVER, 0);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, lbl_radar_info);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 800);
    lv_anim_set_delay(&a, 400);
    lv_anim_set_exec_cb(&a, anim_sent_cb);
    lv_anim_set_ready_cb(&a, on_send_anim_finished);
    lv_anim_start(&a);
}

// --- TOUCH CLICK HANDLER ---

static void click_handler(lv_event_t * e)
{
    LV_UNUSED(e);

    if (current_page == PAGE_AMBIENT) {
        // Tap ambient: enter radar and play current weather soundscape
        set_page(PAGE_RADAR);
        event_post(EVT_TOUCH_CLICK, EV_PRIO_CRITICAL, 0, NULL);
    } else if (current_page == PAGE_RADAR) {
        // Tap radar: send to locked friend (if any)
        if (locked_friend >= 0) {
            event_post(EVT_ENCODER_CLICK, EV_PRIO_CRITICAL,
                       locked_friend, NULL);
        }
        trigger_send();
    }
}

// --- UI INITIALIZATION ---

void knob_init_system(void)
{
    scr_main = lv_scr_act();

    lv_obj_set_style_bg_color(scr_main, lv_color_black(), 0);
    lv_obj_set_style_bg_opa  (scr_main, LV_OPA_COVER,    0);

    // ---------- Ambient layer ----------
    layer_ambient = lv_obj_create(scr_main);
    lv_obj_set_size(layer_ambient, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(layer_ambient, lv_color_black(), 0);
    lv_obj_set_style_border_width(layer_ambient, 0, 0);
    lv_obj_set_style_pad_all(layer_ambient, 0, 0);
    lv_obj_clear_flag(layer_ambient, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t *lbl_title = lv_label_create(layer_ambient);
    lv_label_set_text(lbl_title, "Carecast");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_title,
                                lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 10);

    // Subtitle
    lv_obj_t *lbl_sub = lv_label_create(layer_ambient);
    lv_label_set_text(lbl_sub, "Next hour soundscape");
    lv_obj_set_style_text_font(lbl_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_sub,
                                lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(lbl_sub, LV_ALIGN_TOP_MID, 0, 30);

    // Temperature
    lbl_my_temp = lv_label_create(layer_ambient);
    lv_label_set_text(lbl_my_temp, "--");
    lv_obj_set_style_text_font(lbl_my_temp, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_my_temp, lv_color_white(), 0);
    lv_obj_align(lbl_my_temp, LV_ALIGN_CENTER, 0, -20);

    // City
    lbl_my_city = lv_label_create(layer_ambient);
    lv_label_set_text(lbl_my_city, "Loading...");
    lv_obj_set_style_text_font(lbl_my_city, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(lbl_my_city,
                                lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(lbl_my_city, LV_ALIGN_CENTER, 0, 30);

    // ---------- Radar layer ----------
    layer_radar = lv_obj_create(scr_main);
    lv_obj_set_size(layer_radar, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(layer_radar, lv_color_black(), 0);
    lv_obj_set_style_border_width(layer_radar, 0, 0);
    lv_obj_set_style_pad_all(layer_radar, 0, 0);
    lv_obj_clear_flag(layer_radar, LV_OBJ_FLAG_SCROLLABLE);

    // Soft background circle
    lv_obj_t *bg_circle = lv_obj_create(layer_radar);
    lv_obj_set_size(bg_circle, EDGE_RADIUS * 2 - 30, EDGE_RADIUS * 2 - 30);
    lv_obj_set_style_radius(bg_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(bg_circle, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(bg_circle,
                              lv_palette_darken(LV_PALETTE_BLUE, 2), 0);
    lv_obj_set_style_border_width(bg_circle, 0, 0);
    lv_obj_center(bg_circle);
    lv_obj_move_background(bg_circle);

    // Orbit circle
    lv_obj_t *orbit = lv_obj_create(layer_radar);
    lv_obj_set_size(orbit, EDGE_RADIUS * 2, EDGE_RADIUS * 2);
    lv_obj_set_style_radius(orbit, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(orbit, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(orbit, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(orbit, 1, 0);
    lv_obj_center(orbit);

    // Cursor ring
    cursor_ring = lv_obj_create(layer_radar);
    lv_obj_set_size(cursor_ring, 24, 24);
    lv_obj_set_style_radius(cursor_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(cursor_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(cursor_ring, lv_color_white(), 0);
    lv_obj_set_style_border_width(cursor_ring, 3, 0);
    lv_obj_align(cursor_ring, LV_ALIGN_CENTER, 0, -EDGE_RADIUS);

    // Radar centre label
    lbl_radar_info = lv_label_create(layer_radar);
    lv_label_set_text(lbl_radar_info, "Friend Search");
    lv_obj_set_style_text_font(lbl_radar_info, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(lbl_radar_info, lv_color_white(), 0);
    lv_obj_center(lbl_radar_info);

    // Friend dots + glows
    for (int i = 0; i < 2; i++) {
        friends[i].bearing =
            calculate_bearing(MY_LAT, MY_LON,
                              friends[i].lat, friends[i].lon);

        int x, y;
        get_xy(friends[i].bearing, EDGE_RADIUS, &x, &y);

        // Glow circle (behind)
        friends[i].ui_glow = lv_obj_create(layer_radar);
        lv_obj_set_size(friends[i].ui_glow, 20, 20);
        lv_obj_set_style_radius(friends[i].ui_glow, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(friends[i].ui_glow,
                                  friends[i].base_color, 0);
        lv_obj_set_style_bg_opa(friends[i].ui_glow, LV_OPA_40, 0);
        lv_obj_set_style_border_width(friends[i].ui_glow, 0, 0);
        lv_obj_align(friends[i].ui_glow, LV_ALIGN_CENTER, x, y);
        lv_obj_add_flag(friends[i].ui_glow, LV_OBJ_FLAG_HIDDEN);

        // Main dot
        friends[i].ui_dot = lv_obj_create(layer_radar);
        lv_obj_set_size(friends[i].ui_dot, 12, 12);
        lv_obj_set_style_radius(friends[i].ui_dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(friends[i].ui_dot,
                                  friends[i].base_color, 0);
        lv_obj_set_style_border_width(friends[i].ui_dot, 0, 0);
        lv_obj_align(friends[i].ui_dot, LV_ALIGN_CENTER, x, y);
    }

    // Invisible full‑screen button for taps (must be last = on top)
    full_screen_btn = lv_btn_create(scr_main);
    lv_obj_set_size(full_screen_btn, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_opa(full_screen_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(full_screen_btn, 0, 0);
    lv_obj_add_event_cb(full_screen_btn, click_handler,
                        LV_EVENT_CLICKED, NULL);

    set_page(PAGE_AMBIENT);
}

// --- ENCODER LOGIC ENTRY ---

void knob_change(knob_event_t k, int cont)
{
    LV_UNUSED(cont);
    uint32_t now = lv_tick_get();

    // 1. timeout back from radar
    if (current_page == PAGE_RADAR &&
        (now - last_input_time > TIMEOUT_MS) &&
        !is_sending)
    {
        set_page(PAGE_AMBIENT);
        return;
    }

    // 2. ambient: rotate to enter radar
    if (current_page == PAGE_AMBIENT) {
        if (k == KNOB_LEFT || k == KNOB_RIGHT) {
            set_page(PAGE_RADAR);
        }
        return;
    }

    // 3. radar rotation
    if (current_page == PAGE_RADAR && !is_sending) {
        last_input_time = now;

        if (k == KNOB_LEFT)  radar_angle -= 5;
        if (k == KNOB_RIGHT) radar_angle += 5;

        if (radar_angle < 0)    radar_angle += 360;
        if (radar_angle >= 360) radar_angle -= 360;

        int cx, cy;
        get_xy(radar_angle, EDGE_RADIUS, &cx, &cy);
        lv_obj_align(cursor_ring, LV_ALIGN_CENTER, cx, cy);

        // snapping to friends
        int closest = -1;
        for (int i = 0; i < 2; i++) {
            int dist = abs(radar_angle - friends[i].bearing);
            if (dist > 180) dist = 360 - dist;
            if (dist < SNAP_ANGLE) closest = i;
        }

        if (closest != -1) {
            locked_friend = closest;

            lv_color_t c = friends[closest].base_color;
            lv_obj_set_style_border_color(cursor_ring, c, 0);
            lv_obj_set_style_bg_color(friends[closest].ui_dot, c, 0);
            
            // Bigger dot + show glow
            lv_obj_set_size(friends[closest].ui_dot, 18, 18);
            lv_obj_set_size(friends[closest].ui_glow, 30, 30);
            lv_obj_clear_flag(friends[closest].ui_glow, LV_OBJ_FLAG_HIDDEN);

            // CRITICAL: always reset text opacity to full when showing friend info
            lv_obj_set_style_text_opa(lbl_radar_info, LV_OPA_COVER, 0);
            
            // show full info: name + temp + city
            lv_label_set_text_fmt(lbl_radar_info, "%s\n%d C\n%s",
                                  friends[closest].name,
                                  friends[closest].temp,
                                  friends[closest].city);

            // background follows friend's weather
            apply_bg_color_from_friend(closest);

            // post share event
            event_post(EVT_ENCODER_CLICK, EV_PRIO_CRITICAL,
                       locked_friend, NULL);
        } else {
            locked_friend = -1;
            lv_obj_set_style_border_color(cursor_ring, lv_color_white(), 0);
            
            // Reset opacity before changing text
            lv_obj_set_style_text_opa(lbl_radar_info, LV_OPA_COVER, 0);
            lv_label_set_text(lbl_radar_info, "Friend Search");

            for (int i = 0; i < 2; i++) {
                lv_obj_set_size(friends[i].ui_dot, 12, 12);
                lv_obj_add_flag(friends[i].ui_glow, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_bg_color(friends[i].ui_dot,
                                          friends[i].base_color, 0);
            }

            // revert background to your own weather color
            apply_bg_color_from_my_temp(g_my_temp_last);
        }

        // optional: send angle as high‑level event
        if (k == KNOB_LEFT || k == KNOB_RIGHT) {
            event_post(EVT_ENCODER_ROTATE, EV_PRIO_CRITICAL,
                       radar_angle, NULL);
        }
    }
}

// --- WEATHER DATA INJECTION ---

void update_my_weather(int temp, const char* name)
{
    g_my_temp_last = temp;

    if (current_page == PAGE_AMBIENT) {
        lv_label_set_text_fmt(lbl_my_temp, "%d C", temp);
        lv_label_set_text(lbl_my_city, name);
    }

    // update background color
    apply_bg_color_from_my_temp(temp);
}

void update_friend_weather(int id, int temp, const char* name)
{
    if (id >= 0 && id < 2) {
        friends[id].temp = temp;
        if (name && name[0] != '\0') {
            friends[id].city = name;
        }
    }
}
