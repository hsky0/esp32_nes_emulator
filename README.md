# ESP32 移植nes模拟器

使用esp32 WROOM模组
资料：参照esp关于nes模拟器移植的官方文档：https://github.com/espressif/esp32-nesemu


- master分支：
完成了nes文件的加载，游戏的显示，但是还未接入控制器和声音播放器

- fc_controler分支：
增加fc控制器，实现游戏的控制


- audio分支：
增加音频播放功能
使用 3.5mm CJMCUTRRS 模块外接耳机实现音频播放，使用pdm模式，因为版本变更，未能使用ESP32内置的DAC，使用单个GPIO口传输音频数据，因此导致输出音频噪声很大，后续打算采用es8311驱动音频


