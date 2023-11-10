#include "displayapp/screens/WatchFaceCustom.h"

#include <lvgl/lvgl.h>
#include <cstdio>
#include "displayapp/screens/NotificationIcon.h"
#include "displayapp/screens/Symbols.h"
#include "components/battery/BatteryController.h"
#include "components/ble/BleController.h"
#include "components/ble/NotificationManager.h"
#include "components/heartrate/HeartRateController.h"
#include "components/motion/MotionController.h"
#include "components/settings/Settings.h"
#include "components/ble/weather/WeatherService.h"

using namespace Pinetime::Applications::Screens;

WatchFaceCustom::WatchFaceCustom(Controllers::DateTime& dateTimeController,
                                   const Controllers::Battery& batteryController,
                                   const Controllers::Ble& bleController,
                                   Controllers::NotificationManager& notificationManager,
                                   Controllers::Settings& settingsController,
                                   Controllers::HeartRateController& heartRateController,
                                   Controllers::MotionController& motionController,
                                   Controllers::WeatherService& weatherService)
  : currentDateTime {{}},
    dateTimeController {dateTimeController},
    notificationManager {notificationManager},
    settingsController {settingsController},
    heartRateController {heartRateController},
    motionController {motionController},
    weatherService {weatherService},
    statusIcons(batteryController, bleController) {

  statusIcons.Create();

  notificationIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(notificationIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_LIME);
  lv_label_set_text_static(notificationIcon, NotificationIcon::GetIcon(false));
  lv_obj_align(notificationIcon, nullptr, LV_ALIGN_IN_TOP_LEFT, 0, 0);

  label_date = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_align(label_date, lv_scr_act(), LV_ALIGN_CENTER, 0, 60);
  lv_obj_set_style_local_text_color(label_date, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x999999));

  label_time = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(label_time, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &jetbrains_mono_extrabold_compressed);

  lv_obj_align(label_time, lv_scr_act(), LV_ALIGN_IN_RIGHT_MID, 0, 0);

  label_time_ampm = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(label_time_ampm, "");
  lv_obj_align(label_time_ampm, lv_scr_act(), LV_ALIGN_IN_RIGHT_MID, -30, -55);

  // weather symbol
  weatherIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(weatherIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xFFFF00));
  lv_label_set_text_static(weatherIcon, Symbols::sun);
  lv_obj_align(weatherIcon, lv_scr_act(), LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_local_text_font(weatherIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &fontawesome_weathericons);

  // temperature value
  tempValue = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(tempValue, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xFFFF00));
  lv_label_set_text_static(tempValue, "72");
  lv_obj_align(tempValue, weatherIcon, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  stepValue = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(stepValue, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x00FFE7));
  lv_label_set_text_static(stepValue, "0");
  lv_obj_align(stepValue, lv_scr_act(), LV_ALIGN_IN_BOTTOM_RIGHT, 0, 0);

  stepIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(stepIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x00FFE7));
  lv_label_set_text_static(stepIcon, Symbols::shoe);
  lv_obj_align(stepIcon, stepValue, LV_ALIGN_OUT_LEFT_MID, -5, 0);

  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
  Refresh();
}

WatchFaceCustom::~WatchFaceCustom() {
  lv_task_del(taskRefresh);
  lv_obj_clean(lv_scr_act());
}

void WatchFaceCustom::Refresh() {
  statusIcons.Update();

  notificationState = notificationManager.AreNewNotificationsAvailable();
  if (notificationState.IsUpdated()) {
    lv_label_set_text_static(notificationIcon, NotificationIcon::GetIcon(notificationState.Get()));
  }

  currentDateTime = std::chrono::time_point_cast<std::chrono::minutes>(dateTimeController.CurrentDateTime());

  if (currentDateTime.IsUpdated()) {
    uint8_t hour = dateTimeController.Hours();
    uint8_t minute = dateTimeController.Minutes();

    if (settingsController.GetClockType() == Controllers::Settings::ClockType::H12) {
      char ampmChar[3] = "AM";
      if (hour == 0) {
        hour = 12;
      } else if (hour == 12) {
        ampmChar[0] = 'P';
      } else if (hour > 12) {
        hour = hour - 12;
        ampmChar[0] = 'P';
      }
      lv_label_set_text(label_time_ampm, ampmChar);
      lv_label_set_text_fmt(label_time, "%2d:%02d", hour, minute);
      lv_obj_align(label_time, lv_scr_act(), LV_ALIGN_IN_RIGHT_MID, 0, 0);
    } else {
      lv_label_set_text_fmt(label_time, "%02d:%02d", hour, minute);
      lv_obj_align(label_time, lv_scr_act(), LV_ALIGN_CENTER, 0, 0);
    }

    currentDate = std::chrono::time_point_cast<days>(currentDateTime.Get());
    if (currentDate.IsUpdated()) {
      uint16_t year = dateTimeController.Year();
      uint8_t day = dateTimeController.Day();
      if (settingsController.GetClockType() == Controllers::Settings::ClockType::H24) {
        lv_label_set_text_fmt(label_date,
                              "%s %d %s %d",
                              dateTimeController.DayOfWeekShortToString(),
                              day,
                              dateTimeController.MonthShortToString(),
                              year);
      } else {
        lv_label_set_text_fmt(label_date,
                              "%s %s %d %d",
                              dateTimeController.DayOfWeekShortToString(),
                              dateTimeController.MonthShortToString(),
                              day,
                              year);
      }
      lv_obj_realign(label_date);
    }
  }

  // set the weather symbol and temperature value
  if (weatherService.GetCurrentTemperature()->timestamp != 0 && weatherService.GetCurrentClouds()->timestamp != 0 &&
      weatherService.GetCurrentPrecipitation()->timestamp != 0) {
    nowTemp = ((weatherService.GetCurrentTemperature()->temperature / 100) * (9/5)) + 32;
    clouds = (weatherService.GetCurrentClouds()->amount);
    precip = (weatherService.GetCurrentPrecipitation()->amount);
    if (nowTemp.IsUpdated()) {
      lv_label_set_text_fmt(tempValue, "%d°", nowTemp.Get());
      if ((clouds <= 30) && (precip == 0)) {
        lv_label_set_text(weatherIcon, Symbols::sun);
      } else if ((clouds >= 70) && (clouds <= 90) && (precip == 1)) {
        lv_label_set_text(weatherIcon, Symbols::cloudSunRain);
      } else if ((clouds > 90) && (precip == 0)) {
        lv_label_set_text(weatherIcon, Symbols::cloud);
      } else if ((clouds > 70) && (precip >= 2)) {
        lv_label_set_text(weatherIcon, Symbols::cloudShowersHeavy);
      } else {
        lv_label_set_text(weatherIcon, Symbols::cloudSun);
      };
      lv_obj_realign(tempValue);
      lv_obj_realign(weatherIcon);
    }
  } else {
    lv_label_set_text_static(tempValue, "--");
    lv_label_set_text(weatherIcon, Symbols::ban);
    lv_obj_realign(tempValue);
    lv_obj_realign(weatherIcon);
  }
}
