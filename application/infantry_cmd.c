/****************************************************************************
 *  Copyright (C) 2019 RoboMaster.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 ***************************************************************************/

#include "board.h"
#include "dbus.h"
#include "chassis.h"
#include "gimbal.h"
#include "shoot.h"

#include "init.h"
#include "infantry_cmd.h"
#include "chassis_task.h"
#include "gimbal_task.h"

#include "protocol.h"
#include "referee_system.h"

#define MANIFOLD2_CHASSIS_SIGNAL (1 << 0)
#define MANIFOLD2_GIMBAL_SIGNAL (1 << 1)
#define MANIFOLD2_SHOOT_SIGNAL (1 << 2)
#define MANIFOLD2_FRICTION_SIGNAL (1 << 3)
#define MANIFOLD2_CHASSIS_ACC_SIGNAL (1 << 4)

extern osThreadId cmd_task_t;

struct cmd_gimbal_info cmd_gimbal_info;
struct cmd_chassis_info cmd_chassis_info;
struct manifold_cmd manifold_cmd;

struct manifold_cmd *get_manifold_cmd(void)
{
  return &manifold_cmd;
}

int32_t gimbal_info_rcv(uint8_t *buff, uint16_t len);
int32_t chassis_speed_ctrl(uint8_t *buff, uint16_t len);
int32_t chassis_spd_acc_ctrl(uint8_t *buff, uint16_t len);
int32_t shoot_firction_ctrl(uint8_t *buff, uint16_t len);
int32_t gimbal_angle_ctrl(uint8_t *buff, uint16_t len);
int32_t shoot_ctrl(uint8_t *buff, uint16_t len);
int32_t student_data_transmit(uint8_t *buff, uint16_t len);

int32_t rc_data_forword_by_can(uint8_t *buff, uint16_t len)
{
  protocol_send(GIMBAL_ADDRESS, CMD_RC_DATA_FORWORD, buff, len);
  return 0;
}

int32_t gimbal_adjust_cmd(uint8_t *buff, uint16_t len)
{
  gimbal_auto_adjust_start();
  return 0;
}

/**Added by Y.H. Liu
 * @Jul 13, 2019: define js variables
 * 
 * for chassis power debugging via gimbal
 */
uint8_t current_excess, low_voltage;
int32_t current_detecting_js, voltage_detecting_js, buffer_remained_js, shooter_heat0_js, shooter_heat1_js, robot_level_js;

/** Edited by Y.H. Liu
 *  @Jun 12, 2019: disbable the auto mode and implement the auto_aiming
 *
 *  Implement the customized control logic described in Control.md
 */
float auto_aiming_pitch = 0;
float auto_aiming_yaw   = 0;
/* Edited By Eric Chen
 * Added time gap between two frames to calc speed.
 * July 23, 2019.
 */
uint32_t time_pc = 0;

void infantry_cmd_task(void const *argument)
{
  uint8_t app;
  osEvent event;
  app = get_sys_cfg();

  rc_device_t prc_dev = NULL;
  shoot_t pshoot = NULL;
  gimbal_t pgimbal = NULL;
  chassis_t pchassis = NULL;

  pshoot = shoot_find("shoot");
  pgimbal = gimbal_find("gimbal");
  pchassis = chassis_find("chassis");

  if (app == CHASSIS_APP)
  {
    prc_dev = rc_device_find("uart_rc");
    protocol_rcv_cmd_register(CMD_STUDENT_DATA, student_data_transmit);
    protocol_rcv_cmd_register(CMD_PUSH_GIMBAL_INFO, gimbal_info_rcv);
    protocol_rcv_cmd_register(CMD_SET_CHASSIS_SPEED, chassis_speed_ctrl);
    protocol_rcv_cmd_register(CMD_SET_CHASSIS_SPD_ACC, chassis_spd_acc_ctrl);
  }
  else
  {
    prc_dev = rc_device_find("can_rc");
    protocol_rcv_cmd_register(CMD_SET_GIMBAL_ANGLE, gimbal_angle_ctrl);
    protocol_rcv_cmd_register(CMD_SET_FRICTION_SPEED, shoot_firction_ctrl);
    protocol_rcv_cmd_register(CMD_SET_SHOOT_FREQUENTCY, shoot_ctrl);
    protocol_rcv_cmd_register(CMD_GIMBAL_ADJUST, gimbal_adjust_cmd);
    protocol_rcv_cmd_register(CMD_CHASSIS_POWER, chassis_power_callback);
    protocol_rcv_cmd_register(CMD_SHOOTER_HEAT, shooter_data_callback);
    protocol_rcv_cmd_register(CMD_ROBOT_STATE, robot_state_data_callback);
  }

  while (1)
  {
    if (rc_device_get_state(prc_dev, RC_S2_DOWN) == RM_OK) //S2:DOWN ---- disabled
    {
      memset(&manifold_cmd, 0, sizeof(struct manifold_cmd));
      osDelay(100);
    }
    else
    {
      event = osSignalWait(MANIFOLD2_CHASSIS_SIGNAL | MANIFOLD2_GIMBAL_SIGNAL |
                               MANIFOLD2_SHOOT_SIGNAL | MANIFOLD2_FRICTION_SIGNAL | MANIFOLD2_CHASSIS_ACC_SIGNAL,
                           500);

      if (event.status == osEventSignal)
      {
        if (event.value.signals & MANIFOLD2_CHASSIS_SIGNAL)
        {
          //struct cmd_chassis_speed *pspeed;
          //pspeed = &manifold_cmd.chassis_speed;
          //chassis_set_offset(pchassis, pspeed->rotate_x_offset, pspeed->rotate_x_offset);
          //chassis_set_acc(pchassis, 0, 0, 0);
          //chassis_set_speed(pchassis, pspeed->vx, pspeed->vy, pspeed->vw / 10.0f);
        }

        if (event.value.signals & MANIFOLD2_CHASSIS_ACC_SIGNAL)
        {
          // struct cmd_chassis_spd_acc *pacc;
          // pacc = &manifold_cmd.chassis_spd_acc;
          // chassis_set_offset(pchassis, pacc->rotate_x_offset, pacc->rotate_x_offset);
          // chassis_set_acc(pchassis, pacc->ax, pacc->ay, pacc->wz / 10.0f);
          // chassis_set_speed(pchassis, pacc->vx, pacc->vy, pacc->vw / 10.0f);
        }
				/* Edited By Eric Chen 
				 * Send continous data from PC for Debugging Kalman filter
				 * July 23rd, 2019
				 */
				// For Kalman Debugging
				struct cmd_gimbal_angle *pangle;
				pangle = &manifold_cmd.gimbal_angle;
				auto_aiming_pitch = pangle->pitch;
				auto_aiming_yaw = pangle->yaw; 
				time_pc = pangle->time_pc;
				
				// PC auto aiming mode is enabled
        if ((prc_dev->rc_info.mouse.r || rc_device_get_state(prc_dev, RC_S2_UP) == RM_OK)
          && event.value.signals & MANIFOLD2_GIMBAL_SIGNAL)
        {
          
          //pangle = &manifold_cmd.gimbal_angle;
          if (pangle->ctrl.bit.pitch_mode == 0)
          {
            gimbal_set_pitch_angle(pgimbal, pangle->pitch / 100.0f);
          }
          else
          {
            // gimbal_set_pitch_speed(pgimbal, pangle->pitch / 10.0f);
            auto_aiming_pitch = pangle->pitch;
          }
          if (pangle->ctrl.bit.yaw_mode == 0)
          {
            gimbal_set_yaw_angle(pgimbal, pangle->yaw / 100.0f, 0);
          }
          else
          {
            // gimbal_set_yaw_speed(pgimbal, pangle->yaw / 10.0f);
            auto_aiming_yaw = pangle->yaw;
          }
        }
        //
        // if (event.value.signals & MANIFOLD2_SHOOT_SIGNAL)
        // {
        //   struct cmd_shoot_num *pctrl;
        //   pctrl = &manifold_cmd.shoot_num;
        //   shoot_set_cmd(pshoot, pctrl->shoot_cmd, pctrl->shoot_add_num);
        //   shoot_set_turn_speed(pshoot, pctrl->shoot_freq);
        // }
        //
        // if (event.value.signals & MANIFOLD2_FRICTION_SIGNAL)
        // {
        //   struct cmd_firction_speed *pctrl;
        //   pctrl = &manifold_cmd.firction_speed;
        //   shoot_set_fric_speed(pshoot, pctrl->left, pctrl->right);
        // }
      }
      else
      {
        chassis_set_speed(pchassis, 0, 0, 0);
        chassis_set_acc(pchassis, 0, 0, 0);
        shoot_set_cmd(pshoot, SHOOT_STOP_CMD, 0);
      }
    }
  }
}

int32_t student_data_transmit(uint8_t *buff, uint16_t len)
{
  uint16_t cmd_id = *(uint16_t *)buff;
  referee_protocol_tansmit(cmd_id, buff + 2, len - 2);
  return 0;
}

int32_t chassis_speed_ctrl(uint8_t *buff, uint16_t len)
{
  if (len == sizeof(struct cmd_chassis_speed))
  {
    memcpy(&manifold_cmd.chassis_speed, buff, len);
    osSignalSet(cmd_task_t, MANIFOLD2_CHASSIS_SIGNAL);
  }
  return 0;
}

int32_t chassis_spd_acc_ctrl(uint8_t *buff, uint16_t len)
{
  if (len == sizeof(struct cmd_chassis_spd_acc))
  {
    memcpy(&manifold_cmd.chassis_spd_acc, buff, len);
    osSignalSet(cmd_task_t, MANIFOLD2_CHASSIS_ACC_SIGNAL);
  }
  return 0;
}
//When Gimbal received command from the tx2
// Buff is the address of the received data, check when 
// Buff is envalued
int32_t gimbal_angle_ctrl(uint8_t *buff, uint16_t len)
{
  if (len == sizeof(struct cmd_gimbal_angle))
  {
    memcpy(&manifold_cmd.gimbal_angle, buff, len);
    osSignalSet(cmd_task_t, MANIFOLD2_GIMBAL_SIGNAL);
  }
  return 0;
}

int32_t shoot_firction_ctrl(uint8_t *buff, uint16_t len)
{
  if (len == sizeof(struct cmd_firction_speed))
  {
    memcpy(&manifold_cmd.firction_speed, buff, len);
    osSignalSet(cmd_task_t, MANIFOLD2_FRICTION_SIGNAL);
  }
  return 0;
}

int32_t shoot_ctrl(uint8_t *buff, uint16_t len)
{
  if (len == sizeof(struct cmd_shoot_num))
  {
    memcpy(&manifold_cmd.shoot_num, buff, len);
    osSignalSet(cmd_task_t, MANIFOLD2_SHOOT_SIGNAL);
  }
  return 0;
}

int32_t gimbal_info_rcv(uint8_t *buff, uint16_t len)
{
  struct cmd_gimbal_info *info;
  info = (struct cmd_gimbal_info *)buff;
  chassis_set_relative_angle(info->yaw_ecd_angle / 10.0f);
  return 0;
}

int32_t gimbal_push_info(void *argc)
{
  struct gimbal_info info;
  gimbal_t pgimbal = (gimbal_t)argc;
  gimbal_get_info(pgimbal, &info);

  cmd_gimbal_info.mode = info.mode;
  cmd_gimbal_info.pitch_ecd_angle = info.pitch_ecd_angle * 10;
  cmd_gimbal_info.pitch_gyro_angle = info.pitch_gyro_angle * 10;
  cmd_gimbal_info.pitch_rate = info.pitch_rate * 10;
  cmd_gimbal_info.yaw_ecd_angle = info.yaw_ecd_angle * 10;
  cmd_gimbal_info.yaw_gyro_angle = info.yaw_gyro_angle * 10;
  cmd_gimbal_info.yaw_rate = info.yaw_rate * 10;

  if (get_gimbal_init_state() == 0)
  {
    cmd_gimbal_info.yaw_ecd_angle = 0;
  }

  protocol_send(PROTOCOL_BROADCAST_ADDR, CMD_PUSH_GIMBAL_INFO, &cmd_gimbal_info, sizeof(cmd_gimbal_info));

  return 0;
}

int32_t chassis_push_info(void *argc)
{
  struct chassis_info info;
  chassis_t pchassis = (chassis_t)argc;
  chassis_get_info(pchassis, &info);

  cmd_chassis_info.angle_deg = info.angle_deg * 10;
  cmd_chassis_info.gyro_angle = info.yaw_gyro_angle * 10;
  cmd_chassis_info.gyro_palstance = info.yaw_gyro_rate * 10;
  cmd_chassis_info.position_x_mm = info.position_x_mm;
  cmd_chassis_info.position_y_mm = info.position_y_mm;
  cmd_chassis_info.v_x_mm = info.v_x_mm;
  cmd_chassis_info.v_y_mm = info.v_y_mm;

  protocol_send(MANIFOLD2_ADDRESS, CMD_PUSH_CHASSIS_INFO, &cmd_chassis_info, sizeof(cmd_chassis_info));

  return 0;
}

/**Added by Y.H. Liu
 * @Jul 13, 2019: Declare the function
 * 
 * Send the chassis current data
 */
int32_t power_data_sent_by_can(uint8_t current_flag, uint8_t voltage_flag, float current, float voltage, float buffer)
{
  struct chassis_power_data_t chassis_power_data = {current_flag, voltage_flag, current, voltage, buffer};
  protocol_send(GIMBAL_ADDRESS, CMD_CHASSIS_POWER, &chassis_power_data, sizeof(struct chassis_power_data_t));
  return RM_OK;
}
/**Added by Y.H. Liu
 * @Jul 13, 2019: Declare the function
 * 
 * Callback for chassis_power info from chassis board
 */
int32_t chassis_power_callback(uint8_t *buff, uint16_t len)
{
  if(len == sizeof(struct chassis_power_data_t))
  {
    struct chassis_power_data_t * pchassis_power = (struct chassis_power_data_t *) buff; 
    current_excess = pchassis_power->current_flag;
    low_voltage = pchassis_power->voltage_flag;
    current_detecting_js = pchassis_power->current;
    voltage_detecting_js = pchassis_power->voltage;
    buffer_remained_js = pchassis_power->buffer;
  }
	return RM_OK;
}
/**Added by Y.H. Liu
 * @Jul 20, 2019: Declare the function
 * 
 * Data transmission between chassis and gimbal regarding shooter heat
 */
int32_t shooter_data_sent_by_can(ext_power_heat_data_t * heat_power_d)
{
  protocol_send(GIMBAL_ADDRESS, CMD_SHOOTER_HEAT, &(heat_power_d->shooter_heat0), 2*sizeof(uint16_t));
  return RM_OK;
}
static uint16_t shooter_heat_data[2] = {0};
int32_t shooter_data_callback(uint8_t *buff, uint16_t len)
{
  if(len == 2*sizeof(uint16_t))
  {
    shooter_heat_data[0] = *(uint16_t *)buff;
    shooter_heat_data[1] = *(uint16_t *)(buff+2);
    shooter_heat0_js = shooter_heat_data[0];
    shooter_heat1_js = shooter_heat_data[1];
  }
  return RM_OK;
}
uint16_t * shooter_heat_get_via_can(void)
{
  return shooter_heat_data;
}
/**Added by Y.H. Liu
 * @Jul 21, 2019: Define the functions
 * 
 * Data transmission between chassis and gimbal regarding the robot state
 */
int32_t robot_state_sent_by_can(ext_game_robot_state_t * robot_state_d)
{
  protocol_send(GIMBAL_ADDRESS, CMD_ROBOT_STATE, robot_state_d, sizeof(ext_game_robot_state_t));
  return RM_OK;
}
static uint8_t robot_level=0;
int32_t robot_state_data_callback(uint8_t *buff, uint16_t len)
{
  if(len==sizeof(ext_game_robot_state_t))
  {
    ext_game_robot_state_t * temp = (ext_game_robot_state_t *) buff;
    robot_level = temp->robot_level;
  }
  return RM_OK;
}
uint8_t get_robot_level(void)
{
  robot_level_js = robot_level;
  return robot_level;
}
