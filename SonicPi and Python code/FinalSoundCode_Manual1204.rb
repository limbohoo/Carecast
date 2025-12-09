use_bpm 70

# 初始狀態（之後由 Python OSC 控制）
set :main, "mist"          # 天氣主類別 → Clear / Clouds / Rain / Snow / Drizzle / Thunderstorm / Mist…
set :wind_speed, 0          # 通常 0-17 m/s <5微風, <10清風, <17強風, 17-32輕度颱風
set :wind_gust,  0          # 通常 0–35 m/s（比 wind_speed 更大）
set :wind_deg,   55          # 0–360° → 0=北, 90=東, 180=南, 270=西
set :rain_mm, 0
set :clouds,90              # 雲量 (%) → 0–100%
set :humidity,   100         # 0–100%
set :temp_k,     289.15


#天氣狀態更新（每 0.2 秒更新一次）

live_loop :weather_choose do
  main = get(:main)
  wind = get(:wind_speed)
  
  if main == "clear" && wind > 8
    set(:weather_state, "windy")
  elsif main == "drizzle"
    set(:weather_state, "rain")
  else
    set(:weather_state, main)
  end
  
  sleep 1
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
  sync :drizzle
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
  sync :drizzle
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
  wind = 4
  
  if wind < 3
    sleep 1
    next
  end
  
  wind_amp = map_val(wind, 0, 20, 0.3, 1.2)
  cutoff   = map_val(wind, 3, 15, 70, 110)
  
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
# weather 1 clear bird+pad
#--------------------------------------------

live_loop :clear_bird do
  if get(:weather_state) != "clear"
    sleep 1
    next
  end
  
  sample_path = "D:/sonic pi/sound sample/woodpecker-and-other-birds-on-a-spring-morning.wav"
  
  
  with_fx :compressor, pre_amp: 1.2, threshold: 0.6, slope_above: 0.4 do
    with_fx :hpf, cutoff: 70 do     # ⭐ 去掉低頻 noise（風聲、麥克風 rumble）
      with_fx :eq, low: -1.0, low_mid: -0.5 do   # ⭐ 再切掉混濁噪音
        sample sample_path,
          amp: 11,
          rate: 0.7,
          attack: 0.2,
          release: 1
      end
    end
  end
  
  sleep sample_duration(sample_path) * 1.1
end

live_loop :clear_pad do
  if get(:weather_state) != "clear"
    sleep 1
    next
  end
  
  use_synth :pretty_bell
  
  chords = (ring \
            chord(:f4, :major7),   # 1
            chord(:g4, :dom7),     # 2
            chord(:a3, :minor7),   # 3 ← 上升序列完成
            chord(:e4, :minor7),
            chord(:f4, :major7),
            chord(:c4, :major7),
            chord(:f4, :major7),
            chord(:g4, :dom7)
            )
  
  durations = (ring 3, 3, 3, 4, 4, 6, 4)
  
  ch  = chords.tick
  dur = durations.look
  
  play ch, attack:2,release: dur + 2.5, amp: 0.9
  sleep dur
end


#--------------------------------------------
# weather 2 clouds base+pad
#--------------------------------------------

live_loop :clouds_base do
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
  
  sleep 4
end


rain_chords = (ring \
               chord(:e3, :minor7),
               chord(:c3, :major7),
               chord(:a3, :minor7),
               chord(:b2, :dom7)
               )

live_loop :rain_pad do
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
  
  sleep 8
end

#--------------------------------------------
# weather 4 gust,bg,swirls
#--------------------------------------------
chords = (ring \
          chord(:d3, :minor7),
          chord(:c3, :major7),
          chord(:g3, :sus2),
          chord(:a3, :dom7)
          )

live_loop :windy_bg do
  if get(:weather_state) != "windy"
    sleep 1
    next
  end
  ch = chords.tick
  use_synth :prophet
  with_fx :reverb, room: 0.9, mix: 0.7 do
    with_fx :lpf, cutoff:  100 do
      play ch, amp: 0.1 , attack: 2.0, sustain: 4.0, release: 3.0
    end
  end
  
  sleep 6
end

live_loop :windy_swirls do
  
  if get(:weather_state) != "windy"
    sleep 1
    next
  end
  wind_speed = get(:wind_speed)
  amp_val     = map_val(wind_speed, 0, 30, 1, 2)
  ch = chords.look
  use_synth :hollow
  
  steps = 1 + rrand_i(0, 2)
  steps.times do
    note = ch.choose + [0, 12].choose
    amp  = amp_val
    with_fx :reverb, room: 0.95, mix: 0.7 do
      play note,
        amp: amp_val,
        attack: 0.5,
        sustain: rrand(0.8, 1.6),
        release: 1.5
    end
    sleep rrand(0.4, 0.9)
  end
  sleep 2
end

#--------------------------------------------
# weather 5 mist
#--------------------------------------------

define :set_clouds do |i|
  set :clouds, [[i,1].min,0].max
end

live_loop :mist_dark_amb do
  if get(:weather_state) != "mist"
    sleep 1
    next
  end
  
  clouds = get(:clouds)
  
  with_fx :reverb, room: 1, mix: 0.4 do   # 降低 mix
    use_synth :dark_ambience
    note = (ring :e2, :g2, :d2).choose
    amp_val = [0.05 + 0.18 * clouds, 0.3].min
    pan_val = rrand(-0.4, 0.4)
    play note,
      amp: amp_val,
      pan: pan_val,
      attack: 2.0,
      sustain: 2.0 + 2.0 * clouds,
      release: 3.0
  end
  
  sleep 3.5 + 2.0 * clouds   # 稍微延長
end


#--------------------------------------------
# 背景 drone：ambi_drone，偏向一侧，然后下次换另一侧
#--------------------------------------------

live_loop :mist_drone do
  if get(:weather_state) != "mist"
    sleep 1
    next
  end
  
  clouds = get(:clouds)
  
  with_fx :reverb, room: 0.9, mix: 0.3 do
    if rand < (0.3 + 0.4 * clouds)
      pan_val = (ring -0.3, 0.3).choose
      with_fx :hpf, cutoff: 70 do
        sample :ambi_drone,
          amp: clouds*0.02,
          rate: 0.8,
          pan: pan_val
      end
    end
  end
  
  sleep 1 + rrand(1, 3)
end


#--------------------------------------------
# 高频“颗粒”：更明显的左右飘动
#--------------------------------------------
live_loop :mist_particles do
  if get(:weather_state) != "mist"
    sleep 1
    next
  end
  
  clouds = get(:clouds)
  
  with_fx :echo, phase: 0.75, decay: 2.5, mix: 0.3 do
    if rand < (0.35 * clouds)
      use_synth :hollow
      pan_val = rrand(-0.6, 0.6)
      amp_val = [1.2 + 0.8 * clouds, 2.0].min   # 限制最大音量
      play (scale :e4, :minor_pentatonic).choose,
        amp: amp_val,
        pan: pan_val,
        attack: 0.5,
        release: 2.0
    end
  end
  
  sleep rrand(2.0, 3.5)   # 稍微拉長間隔，避免重疊
end