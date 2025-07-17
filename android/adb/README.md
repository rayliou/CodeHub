
# android 文件同步
## ncdu
Visit the website for more information: https://dev.yorhel.nl/ncdu

### Pixel 9 Pro XL
Push the ncdu binary to the device
```
adb push ncdu /data/local/tmp/ncdu
```

Run the ncdu binary
```
adb shell -t  /data/local/tmp/ncdu /storage/emulated/0
```
