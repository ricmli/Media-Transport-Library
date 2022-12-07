/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include "linux-mtl.h"
#if LIBOBS_API_MAJOR_VER == 28
#include <QtGui/QAction>
#else
#include <QtWidgets/QAction>
#endif
#include <obs/obs-frontend-api.h>

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("linux-mtl", "en-US")
MODULE_EXPORT const char* obs_module_description(void) {
  return "Linux MTL input/output";
}

extern struct obs_source_info mtl_input;
#if defined(TODO_OUTPUT)
extern struct obs_output_info mtl_output;
#endif

bool obs_module_load(void) {
  obs_register_source(&mtl_input);
#if defined(TODO_OUTPUT)
  obs_register_output(&mtl_output);
#endif

  obs_data_t* obs_settings = obs_data_create();

  obs_apply_private_data(obs_settings);
  obs_data_release(obs_settings);

#if defined(TODO_OUTPUT)
  config_t* obs_config = obs_frontend_get_global_config();

  QMainWindow* main_window = (QMainWindow*)obs_frontend_get_main_window();
  QAction* action = (QAction*)obs_frontend_add_tools_menu_qaction("MTL Output");
  QMenu* menu = new QMenu();
  action->setMenu(menu);

  QAction* tools_menu_action = menu->addAction(obs_module_text("Active"));
  tools_menu_action->setCheckable(true);
  tools_menu_action->setChecked(false);

  tools_menu_action->connect(tools_menu_action, &QAction::triggered, [=](bool checked) {
    if (checked) {
      obs_output_set_media(mtl_output, obs_get_video());

      if (!obs_output_start(mtl_output)) {
        obs_output_force_stop(mtl_output);
        tools_menu_action->setChecked(false);

        QMessageBox mb(QMessageBox::Warning, "MTL Output",
                       obs_module_text("OutputStartFailed"), QMessageBox::Ok,
                       main_window);
        mb.setButtonText(QMessageBox::Ok, "OK");
        mb.exec();
      }
    } else {
      obs_output_force_stop(mtl_output);
    }
  });
#endif
  return true;
}

enum st_frame_fmt obs_to_mtl_format(enum video_format fmt) {
  switch (fmt) {
    case VIDEO_FORMAT_UYVY: /* UYVY can be converted from YUV422BE10 */
      return ST_FRAME_FMT_UYVY;
    case VIDEO_FORMAT_NV12:
    case VIDEO_FORMAT_I420:
      return ST_FRAME_FMT_YUV420CUSTOM8;
    case VIDEO_FORMAT_YUY2:
    case VIDEO_FORMAT_YVYU:
      return ST_FRAME_FMT_YUV422CUSTOM8;
    default:
      return ST_FRAME_FMT_MAX;
  }
}

enum st_fps obs_to_mtl_fps(uint32_t fps_num, uint32_t fps_den) {
  switch (fps_num) {
    case 30000:
      return ST_FPS_P29_97;
    case 60000:
      return ST_FPS_P59_94;
    case 30:
      return ST_FPS_P30;
    case 60:
      return ST_FPS_P60;
    case 25:
      return ST_FPS_P25;
    case 24:
      return ST_FPS_P24;
    case 50:
      return ST_FPS_P50;
    default:
      return ST_FPS_MAX;
  }
}
