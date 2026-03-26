# Lilygo T-RGB with LVGL and tft_espi

## Get started

1. rename ```libraries``` folder to  ```lib```
2. rename ```ui.ino``` file locatted in side ui folder  to ```main.cpp``` 
3. rename ```ui``` folder to ```scr```
4. open in in vs code using platformio compile and upload

5. Combile and upload using pio command
```bash
    "C:\Users\ed\.platformio\penv\Scripts\pio.exe" run -d "02-Moon-Phase-Clock-T-RGB/PIO" --target upload
```
6. Serial-Monitor baudrate 115200
```bash
    "C:\Users\ed\.platformio\penv\Scripts\pio.exe" device monitor --baud 115200 --port COM6
```

