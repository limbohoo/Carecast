// audio_manager.h
#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>

// --- 系统控制 ---
bool audio_manager_init();
void audio_set_sd_status(bool status);
void audio_stop();

// --- 基础功能 ---
void audio_play_file(const char* filePath);
bool audio_is_playing();

// --- 业务逻辑 (Event Manager 需要的函数) ---
void audio_play_manual_current();              // 播放当前本地天气
void audio_play_friend_send_confirm();         // 播放“发送成功”音效
void audio_play_friend_weather(int friendId);  // 播放朋友城市的天气
bool audio_can_start_ambient();                // 是否可以插入环境音
void audio_play_ambient_forecast();            // 播放环境预报
void audio_handle_friend_notification(int friendId); // 处理朋友发来的通知
void audio_on_finished();                      // 播放结束时的回调

#endif