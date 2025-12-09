In Conda environment, you need to install these package
- pip install pyserial
- pip install python-osc
- pip install request


Arduino : Save data in Serial
Python : request API from OpenWeather & read serial from arduino → send to OSC
Sonic Pi : receive OSC + respond

STATE: ON					//start playing current weather sound for 15s
STATE: OFF                                      //playing weather forecast for 15s per 60s 
ACTION: SENT					//play sent sound UI sound 1
STATE: MSG_PLAY	                                //play friend 1 sound for 15s, or until next "STATE: MSG_PLAY"                                                  //start playing friend 2 sound for 15s or state off will turn off the sound
ACTION: CHARGING
