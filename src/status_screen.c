#include "active_time.h"
#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zmk/activity.h>
#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/display/widgets/output_status.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/split/central.h>

static lv_obj_t *battery_label;
static lv_obj_t *active_time_label;
static struct zmk_widget_output_status output;
static struct zmk_widget_layer_status layer;

/* LVGL timers run in the display context; never update labels from the event
 * thread. */
static void update_active_time(lv_timer_t *timer) {
  ARG_UNUSED(timer);
  int64_t elapsed = corne_active_time_total();
  uint32_t minutes = elapsed / 60000;
  int64_t right_ms;
  bool fresh;
  if (corne_right_active_time(&right_ms, &fresh)) {
    uint32_t right_minutes = right_ms / 60000;
    lv_label_set_text_fmt(
        active_time_label, "I %lu:%02lu\nD %lu:%02lu%s",
        (unsigned long)(minutes / 60), (unsigned long)(minutes % 60),
        (unsigned long)(right_minutes / 60),
        (unsigned long)(right_minutes % 60), fresh ? "" : " *");
  } else {
    lv_label_set_text_fmt(active_time_label, "I %lu:%02lu\nD --:--",
                          (unsigned long)(minutes / 60),
                          (unsigned long)(minutes % 60));
  }
}

struct battery_state {
  uint8_t left;
  uint8_t right;
  bool right_known;
};

static struct battery_state get_state(const zmk_event_t *eh) {
  static bool right_known;
  uint8_t right = 0;
  const struct zmk_peripheral_battery_state_changed *ev =
      as_zmk_peripheral_battery_state_changed(eh);
  zmk_split_central_get_peripheral_battery_level(0, &right);
  if ((ev && ev->source == 0) || right > 0) {
    right_known = true;
  }
  return (struct battery_state){zmk_battery_state_of_charge(), right,
                                right_known};
}

static void update_battery(struct battery_state state) {
  if (state.right_known) {
    lv_label_set_text_fmt(battery_label, "I:%u%% D:%u%%", state.left,
                          state.right);
  } else {
    lv_label_set_text_fmt(battery_label, "I:%u%% D:--", state.left);
  }
}

ZMK_DISPLAY_WIDGET_LISTENER(corne_batteries, struct battery_state,
                            update_battery, get_state)
ZMK_SUBSCRIPTION(corne_batteries, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(corne_batteries, zmk_peripheral_battery_state_changed);

lv_obj_t *zmk_display_status_screen(void) {
  lv_obj_t *screen = lv_obj_create(NULL);
  zmk_widget_output_status_init(&output, screen);
  lv_obj_align(zmk_widget_output_status_obj(&output), LV_ALIGN_TOP_LEFT, 0, 0);
  zmk_widget_layer_status_init(&layer, screen);
  lv_obj_align(zmk_widget_layer_status_obj(&layer), LV_ALIGN_TOP_RIGHT, 0, 0);
  battery_label = lv_label_create(screen);
  lv_obj_set_style_text_font(battery_label, &lv_font_unscii_8, LV_PART_MAIN);
  lv_obj_align(battery_label, LV_ALIGN_LEFT_MID, 0, 0);
  active_time_label = lv_label_create(screen);
  lv_obj_set_style_text_font(active_time_label, &lv_font_unscii_8,
                             LV_PART_MAIN);
  lv_obj_align(active_time_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  corne_batteries_init();
  update_active_time(NULL);
  lv_timer_create(update_active_time, 1000, NULL);
  return screen;
}
