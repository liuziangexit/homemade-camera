# 家酿安防摄像头

<h2>编译</h2>

目前只能在Raspberry Pi OS上编译，需要编译器支持C++ 17
```
./build.sh
```

<h2>配置文件</h2>

```
{
  "codec": "H264",            //编码方式
  "duration": 300,            //单个文件持续秒数
  "save-location": "video",   //保存位置
  "camera-id": 1              //opencv camera id
}
```
