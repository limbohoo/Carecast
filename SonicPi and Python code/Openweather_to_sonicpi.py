import requests
import time
from pythonosc import udp_client


API_KEY = "c5896037834d5748b6f1f1e56d6a2a81"
LAT = 51.44806
LON = 5.48972

OSC_IP = "127.0.0.1"
OSC_PORT = 4560

# Current weather
OWM_URL_CURRENT = f"https://api.openweathermap.org/data/2.5/weather?lat={LAT}&lon={LON}&appid={API_KEY}&units=metric"

# Forecast (3-hour intervals)
OWM_URL_FORECAST = f"https://pro.openweathermap.org/data/2.5/forecast/hourly?lat={LAT}&lon={LON}&appid={API_KEY}&units=metric"

client = udp_client.SimpleUDPClient(OSC_IP, OSC_PORT)
def send_weather():
    try:
        r = requests.get(OWM_URL_CURRENT)
        data = r.json()

        condition = data["weather"][0]["main"].lower()
        temp = float(data["main"]["temp"])
        wind_speed = float(data["wind"]["speed"])
        wind_gust = float(data["wind"].get("gust", 0.0))
        wind_deg = float(data["wind"]["deg"])
        humidity = float(data["main"]["humidity"])
        clouds = float(data["clouds"]["all"])
        rain_mm = data.get("rain", {}).get("1h", 0.0)

        print(f"current/condition = {condition}")
        print(f"current/temp = {temp}")
        print(f"current/wind_speed = {wind_speed}")
        print(f"current/wind_gust = {wind_gust}")
        print(f"current/wind_deg = {wind_deg}")
        print(f"current/humidity = {humidity}")
        print(f"current/clouds = {clouds}")
        print(f"current/rain_mm = {rain_mm}")
        print("-" * 40)

        client.send_message("current/condition", condition)
        client.send_message("current/temp", temp)
        client.send_message("current/wind_speed", wind_speed)
        client.send_message("current/wind_gust", wind_gust)
        client.send_message("current/wind_deg", wind_deg)
        client.send_message("current/humidity", humidity)
        client.send_message("current/clouds", clouds)
        client.send_message("current/rain_mm", rain_mm)
 # Next 1 hr forecast
        f = requests.get(OWM_URL_FORECAST)
        forecast = f.json()
        next1h = forecast["list"][0]  

        f_condition = next1h["weather"][0]["main"].lower()
        f_temp = float(next1h["main"]["temp"])
        f_wind_speed = float(next1h["wind"]["speed"])
        f_wind_gust = float(next1h["wind"].get("gust", 0.0))
        f_wind_deg = float(next1h["wind"]["deg"])
        f_humidity = float(next1h["main"]["humidity"])
        f_clouds = float(next1h["clouds"]["all"])
        f_rain_mm = next1h.get("rain", {}).get("3h", 0.0)

        print("=== Forecast (Next 1h) ===")
        print(f"forecast/condition = {f_condition}")
        print(f"forecast/temp = {f_temp}")
        print(f"forecast/wind_speed = {f_wind_speed}")
        print(f"forecast/wind_gust = {f_wind_gust}")
        print(f"forecast/wind_deg = {f_wind_deg}")
        print(f"forecast/humidity = {f_humidity}")
        print(f"forecast/clouds = {f_clouds}")
        print(f"forecast/rain_mm = {f_rain_mm}")
        print("=" * 40)

        client.send_message("forecast/condition", f_condition)
        client.send_message("forecast/temp", f_temp)
        client.send_message("forecast/wind_speed", f_wind_speed)
        client.send_message("forecast/wind_gust", f_wind_gust)
        client.send_message("forecast/wind_deg", f_wind_deg)
        client.send_message("forecast/humidity", f_humidity)
        client.send_message("forecast/clouds", f_clouds)
        client.send_message("forecast/rain_mm", f_rain_mm)

    except Exception as e:
        print(" API Error:", e)
   
                            
# 每 60 秒更新
while True:
    send_weather()
    time.sleep(120)