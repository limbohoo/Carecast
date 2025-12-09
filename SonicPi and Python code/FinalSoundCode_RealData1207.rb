use_osc "127.0.0.1", 4560

# 接收 Arduino 訊號
# ----------------------------
set :play_mode, false           # 是否正在播放 10 秒
set :play_start, 0              # 播放開始時間
set :use_current, false         # true=使用現在天氣, false=使用未來一小時

set :friend_stage, 0
set :friend_playing, false

live_loop :listener do
  use_real_time
  msg = sync "/osc*/event"
  evt = msg[0]
  puts "#{evt}"
  set :evt, evt
  
  if evt == "STATE: ON"
    set :use_current, true
    set :play_mode, "weather"
    set :play_start, vt
  end
  
  if evt == "STATE: OFF"
    set :use_current, false
    set :play_mode, false       # 停止所有天氣播放
    set :friend_playing, false  # 停止朋友音景
  end
  
  if evt == "ACTION: SENT"
    cue :sent_trigger   # 每按一次就 cue 一次
  end
  
  if evt == "ACTION: CHARGING"
    cue :charging_trigger   # 每按一次就 cue 一次
  end
  
  if evt == "STATE: MSG_PLAY"
    if !get(:friend_playing)
      # 第一次按 → 播放朋友1
      set :friend_stage, 0
      cue :friend1_start
      set :friend_playing, true
      set :friend_start, vt
      
    else
      # 第二次按 → 播放朋友2
      set :friend_stage, 1
      cue :friend2_start
      set :friend_start, vt
    end
  end
  sleep 0.5
end


# 接收「現在天氣」
# ----------------------------
live_loop :current_condition do
  cond = sync "/osc*/current/condition"
  set :main, cond[0]
end

live_loop :current_temp do
  temp = sync "/osc*/current/temp"
  set :temp_c, temp[0]
end

live_loop :current_wind_speed do
  ws = sync "/osc*/current/wind_speed"
  set :wind_speed, ws[0]
end

live_loop :current_wind_gust do
  gust = sync "/osc*/current/wind_gust"
  set :wind_gust, gust[0]
end

live_loop :current_wind_deg do
  wd = sync "/osc*/current/wind_deg"
  set :wind_deg, wd[0]
end

live_loop :current_humidity do
  hum = sync "/osc*/current/humidity"
  set :humidity, hum[0]
end

live_loop :current_clouds do
  cl = sync "/osc*/current/clouds"
  set :clouds, cl[0]
end

live_loop :current_rain do
  rain = sync "/osc*/current/rain_mm"
  set :rain_mm, rain[0]
end

# ----------------------------
# 接收「未來一小時」預報
# ----------------------------
live_loop :forecast_condition do
  cond = sync "/osc*/forecast/condition"
  set :next1hr_main, cond[0]
end

live_loop :forecast_temp do
  temp = sync "/osc*/forecast/temp"
  set :next1hr_temp_c, temp[0]
end

live_loop :forecast_wind_speed do
  ws = sync "/osc*/forecast/wind_speed"
  set :next1hr_wind_speed, ws[0]
end

live_loop :forecast_wind_deg do
  wd = sync "/osc*/forecast/wind_deg"
  set :next1hr_wind_deg, wd[0]
end

live_loop :forecast_humidity do
  hum = sync "/osc*/forecast/humidity"
  set :next1hr_humidity, hum[0]
end

live_loop :forecast_clouds do
  cl = sync "/osc*/forecast/clouds"
  set :next1hr_clouds, cl[0]
end

live_loop :forecast_rain do
  rain = sync "/osc*/forecast/rain_mm"
  set :next1hr_rain_mm, rain[0]
end

live_loop :forecast_wind_gust do
  gust = sync "/osc*/forecast/wind_gust"
  set :next1hr_wind_gust, gust[0]
end

# ----------------------------
# print current and next1hr weather
# ----------------------------
live_loop :print_current_weather do
  sync "/osc*/forecast/temp"
  puts "current/condition = #{get(:main)}"
  puts "current/temp = #{get(:temp_c).round(2)}"
  puts "current/wind_speed = #{get(:wind_speed).round(2)}"
  puts "current/wind_deg = #{get(:wind_deg)}"
  puts "current/wind_gust = #{get(:wind_gust)}"
  puts "current/humidity = #{get(:humidity)}"
  puts "current/clouds = #{get(:clouds)}"
  puts "current/rain_mm = #{get(:rain_mm)}"
  puts "----------------------------------------"
  sleep 0.5
end

live_loop :print_forecast_weather do
  sync "/osc*/forecast/temp"
  puts "next1hr/condition = #{get(:next1hr_main)}"
  puts "next1hr/temp = #{get(:next1hr_temp_c).round(2)}"
  puts "next1hr/wind_speed = #{get(:next1hr_wind_speed).round(2)}"
  puts "next1hr/wind_deg = #{get(:next1hr_wind_deg)}"
  puts "next1hr/wind_gust = #{get(:next1hr_wind_gust)}"
  puts "next1hr/humidity = #{get(:next1hr_humidity)}"
  puts "next1hr/clouds = #{get(:next1hr_clouds)}"
  puts "next1hr/rain_mm = #{get(:next1hr_rain_mm)}"
  puts "========================================"
  sleep 0.5
end

#天氣狀態更新（每 0.2 秒更新一次）

live_loop :weather_choose do
  if get(:use_current) == true
    main = get(:main)
    set :wind_speed, get(:wind_speed)
    set :rain_mm, get(:rain_mm)
    set :clouds, get(:clouds)
    set :humidity, get(:humidity)
    set :wind_deg, get(:wind_deg)
    set :wind_gust, get(:wind_gust)
    set :temp_c, get(:temp_c)
  else
    main = get(:next1hr_main)
    set :wind_speed,get(:next1hr_wind_speed)
    set :rain_mm, get(:next1hr_rain_mm)
    set :clouds, get(:next1hr_clouds)
    set :humidity, get(:next1hr_humidity)
    set :wind_deg, get(:next1hr_wind_deg)
    set :wind_gust, get(:next1hr_wind_gust)
    set :temp_c, get(:next1hr_temp_c)
  end
  
  if main == "clear" && wind > 8
    set(:weather_state, "windy")
  elsif main == "drizzle"
    set(:weather_state, "rain")
  elsif main == "fog"
    set(:weather_state, "mist")
  else
    set(:weather_state, main)
  end
  
  
  sleep 0.5
end

# 「現在」更新 → 如果 use_current = true，就播 15 秒
live_loop :trigger_current_15sec do
  sync "/osc*/current/temp"
  if get(:use_current)
    set :play_mode, "weather"
    set :play_start, vt
    puts "PLAY 15s: CURRENT"
  end
end

# 「未來一小時」更新 → 如果 use_current = false，就播 15 秒
live_loop :trigger_forecast_15sec do
  sync "/osc*/forecast/temp"
  unless get(:use_current)
    set :play_mode, "weather"
    set :play_start, vt
    puts "PLAY 15s: FORECAST"
  end
end

# 小工具：檢查是否還在 15 秒內
define :playing_weather? do
  mode = get(:play_mode)
  start_t = get(:play_start) || 0
  return false if mode != "weather"
  elapsed = vt - start_t
  if elapsed > 15
    set :play_mode, false
    return false
  end
  true
end


# map fuction
define :map_val do |v, in_min, in_max, out_min, out_max|
  return out_min if in_min == in_max  # 避免除以零
  v = [[v, in_min].max, in_max].min  # clamp 在範圍內
  ratio = (v - in_min).to_f / (in_max - in_min).to_f
  out_min + ratio * (out_max - out_min)
end


#--------------------------------------------
#rain drop, overlap with all weather
#--------------------------------------------

define :dual_synth_drop do |s1, s2, note, amp_base|
  unless playing_weather?
    sleep 1
    next
  end
  use_synth s1
  play note,
    amp: amp_base * 0.6,
    attack: 0.01,
    release: 0.35
  use_synth s2
  play note + [0, 2].choose,
    amp: amp_base * 0.4,
    attack: 0.0,
    release: 0.45
end

# 1. Drizzle：高音、稀疏
live_loop :drizzle do
  unless playing_weather?
    sleep 1
    next
  end
  rain_mm  = get(:rain_mm)
  rain_norm = map_val(rain_mm, 0, 30, 0.0, 1.0)
  
  with_fx :echo, phase: 0.45, decay: 2.5, mix: 0.35 do
    note = (scale :e5, :minor_pentatonic).choose
    if rand < (0.9 * (1 - rain_norm))
      dual_synth_drop :sine, :tri, note, rain_norm*0.8
    end
  end
  
  # 雨量映射到 3.0 秒 → 0.2 秒
  sleep map_val(rain_mm, 0, 30, 3.0, 0.2)
end

# 2. Middle rain：中音區
live_loop :middle_rain do
  unless playing_weather?
    sleep 1
    next
  end
  rain_mm  = get(:rain_mm)
  rain_norm = map_val(rain_mm, 0, 30, 0.0, 0.8)
  
  with_fx :echo, phase: 0.28, decay: 2.2, mix: 0.5 do
    if rand < map_val(rain_mm, 0, 30, 0.05, 0.95)
      note = (scale :e4, :minor_pentatonic).choose
      amp_val =  rain_norm*0.8
      dual_synth_drop :tri, :beep, note, amp_val
    end
  end
  
  sleep map_val(rain_mm, 0, 30, 1.5, 0.5)
end

# 3. Heavy rain：低音區
live_loop :heavy_rain do
  unless playing_weather?
    sleep 1
    next
  end
  rain_mm  = get(:rain_mm)
  rain_norm = map_val(rain_mm, 0, 30, 0.0, 1.0)
  
  with_fx :echo, phase: 0.16, decay: 3.5, mix: 0.75 do
    if rand < rain_norm
      low_note = (scale :e3, :minor_pentatonic).choose
      dual_synth_drop :beep, :tri, low_note, rain_norm*0.8
      
      if rand < rain_norm
        use_synth :sine
        play (scale :e5, :minor_pentatonic).choose,
          amp: rain_norm*0.15,
          attack: 0.0,
          release: 0.2
      end
    end
  end
  
  sleep map_val(rain_mm, 0, 30, 0.7, 0.45)
end


#--------------------------------------------
# wind, overlap with all weather
#--------------------------------------------
live_loop :wind do
  wind_speed  = get(:wind_speed)
  unless playing_weather?
    sleep 1
    next
  end
  if wind_speed < 3
    sleep 1
    next
  end
  
  wind_amp = map_val(wind_speed, 0, 20, 0.3, 1.2)
  cutoff   = map_val(wind_speed, 3, 15, 70, 110)
  
  with_fx :reverb, room: 0.9, mix: 0.7 do
    with_fx :lpf, cutoff: cutoff + rrand(-10, 10) do
      synth :noise, amp: wind_amp,
        attack: 1.0,
        sustain: 2.0,
        release: rrand(4.0, 8.0)
    end
  end
  
  sleep rrand(3, 5)
end


live_loop :windy_gust do
  wind_deg = get(:wind_deg)
  wind_gust = get(:wind_gust)
  unless playing_weather?
    sleep 1
    next
  end
  if get(:weather_state) != "windy"
    sleep 1
    next
  end
  
  use_synth :noise   # 白噪音，模擬風的基底
  
  # 根據風向角度計算 pan (-1=左, +1=右)
  pan_val = Math.sin(wind_deg * Math::PI / 180.0)
  
  # 使用 gust 映射到音量、延續時間、間隔
  amp_val     = map_val(wind_gust, 0, 20, 0.03, 0.18)
  release_val = map_val(wind_gust, 0, 30, 1, 5.0)
  sleep_val   = map_val(wind_gust, 0, 30, 7.0, 1.5)
  
  # 音高隨機上下浮動：C3 ~ C5
  note_val = [:c3, :c4, :c5].choose
  
  cutoff_start = rrand(60, 80)
  cutoff_end   = rrand(100, 120)
  
  with_fx :reverb, room: 0.5, mix: 0.7 do
    n = play note_val,
      amp: amp_val,
      attack: 0.1,
      sustain: 0.2,
      release: release_val + rrand(1, 2.5),
      pan: pan_val,
      cutoff: cutoff_start,
      cutoff_slide: 0.3
    
    control n, cutoff: cutoff_end
  end
  
  sleep sleep_val * rrand(1, 2.5)
end


#--------------------------------------------
# weather 2 clouds base+pad
#--------------------------------------------

live_loop :clouds_base do
  unless playing_weather?
    sleep 1
    next
  end
  if get(:weather_state) != "clouds"
    sleep 1
    next
  end
  use_synth :hollow
  notes = [:f1, :f2, :f3]  # F2 上下的音
  with_fx :reverb, room: 0.9, mix: 0.75 do
    play notes.choose, amp: 1, sustain:3,
      release: 8.0
  end
  sleep 1
end

live_loop :clouds_pad do
  unless playing_weather?
    sleep 1
    next
  end
  if get(:weather_state) != "clouds"
    sleep 1
    next
  end
  clouds = get(:clouds) || 0
  clouds_amp  = map_val(clouds, 0, 100, 0.5, 1)
  
  chords = (ring \
            chord(:e3, :minor7),
            chord(:c3, :major7),
            chord(:a3, :minor7),
            chord(:b2, :dom7)
            )
  ch = chords.tick
  
  use_synth :prophet
  with_fx :reverb, room: 0.9, mix: 0.7 do
    with_fx :lpf, cutoff: 95 do
      play ch, amp: clouds_amp ,
        attack: 2.5,
        sustain: 5.5,
        release: 4.0
    end
  end
  sleep rrand(5, 8)
end

#--------------------------------------------
# weather 3 rain,drizzle
#--------------------------------------------

live_loop :rain_base do
  unless playing_weather?
    sleep 1
    next
  end
  if get(:weather_state) != "rain"
    sleep 1
    next
  end
  
  use_synth :dark_ambience
  
  with_fx :reverb, room: 0.9, mix: 0.7 do
    with_fx :lpf, cutoff:  60 do
      play :e2, amp: 3, release: 6
    end
  end
  
  sleep 1
end

rain_chords = (ring \
               chord(:e3, :minor7),
               chord(:c3, :major7),
               chord(:a3, :minor7),
               chord(:b2, :dom7)
               )

live_loop :rain_pad do
  unless playing_weather?
    sleep 1
    next
  end
  if get(:weather_state) != "rain"
    sleep 1
    next
  end
  
  ch = rain_chords.tick
  
  use_synth :prophet
  
  with_fx :reverb, room: 0.9, mix: 0.75 do
    with_fx :lpf, cutoff:75 do
      play ch,
        amp: 2,
        attack: 3.0,
        sustain: 6.0,
        release: 5.0
    end
  end
  
  sleep 1
end

#-------------------------------------------
live_loop :sent_sound do
  sync :sent_trigger    # 收到 cue 就播放
  with_fx :reverb, room: 0.3, mix: 0.5 do
    with_fx :lpf, cutoff: 60, cutoff_slide: 0.18 do |f|
      
      synth :noise,
        amp: 0.5,
        attack: 0.01,
        sustain: 0.2,
        release: 1
      
      control f, cutoff: 110
    end
  end
end

#-------------------------------------------
live_loop :charging1 do
  sync :charging_trigger
  use_synth :hollow
  play :c5, attack: 2, sustain: 2, release: 2, amp: 0.2
  sleep 1
  stop
end

live_loop :charging2 do
  sync :charging_trigger
  use_synth :sine
  play :c2, attack: 2, sustain: 2, release: 2, amp: 0.2
  sleep 1
  stop
end
#-------------------------------------------

# 撥放朋友1
live_loop :friend1_player do
  if get(:friend_stage)==1
    stop
  end
  sync :friend1_start
  sample "D:/sonic pi/sound sample/friend1.wav", amp: 2
end

# 撥放朋友2
live_loop :friend2_player do
  sync :friend2_start
  sample "D:/sonic pi/sound sample/friend2.wav", amp: 4
end

# 計時器：朋友1播滿 15 秒 → 自動接朋友2；朋友2播滿 15 秒 → 停止
live_loop :friend_timer do
  sleep 0.2
  if get(:friend_playing)
    elapsed = vt - get(:friend_start)
    
    if get(:friend_stage) == 0 && elapsed > 15
      # 朋友1播滿 → 自動切到朋友2
      set :friend_stage, 1
      cue :friend2_start
      set :friend_start, vt
      
    elsif get(:friend_stage) == 1 && elapsed > 15
      # 朋友2播滿 → 停止播放
      set :friend_playing, false
    end
  end
end

