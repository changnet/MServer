一些和工程相关的配置、脚本都放这里

PS:
1. 之前一直使用Makefile作为编译脚本，但越来越多的开发工具（Visual Studio、Visual Studio Code、Qt Creator、CLion）都支持CMake，因此必须改用CMake了。
2. 尝试过把CMakeLists.txt放在project目录，但是Qt Creator对此支持不好，左边的目录无法只显示当前项目的目录，搜索也无法搜索头文件，只能识别cpp文件，最后只能放到engine目录了。

## Qt Creator
https://www.qt.io/offline-installers

我主要是在linux下用Visual Studio Code写这个框架，所以一般是不用IDE的。如果一定要用IDE，推荐跨平台的Qt Creator。目前构建使用CMake，因此需要安装CMake，然后用Qt Creator打开CMakeList.txt即可。

### no suitable kit found
如果Qt Creator打开工程提示：no suitable kit found，是因为没有安装qmake，无法解析工程文件

apt install qt5-default，然后在Qt Creator tools->Options->Qt Versions->add，最后设置好`构建套件(Kits)`中的qmake版本即可

如果CMake无法自动检测到，也需要手动设置

### clang-format
在Qt Creator的 帮助->插件里，新版本有clang-format支持。如果不能用（https://bugs.launchpad.net/ubuntu/+source/qtcreator/+bug/1876436），则在Beautifier这个插件里。配置选择File即可，.clang-format配置文件必须在engine/src根目录，否则格式化不生效，也不会有什么错误提示

### 常用快捷方式
* ctrl + 鼠标点击可快速跳转到符号
* Ctrl+F4:Close current file
* Alt+Left:Go back
* Alt+Right:Go forward
* Ctrl+L:Go to line

### 定位器(Locator)
Ctrl+K 直接跳转到定位器，即左下的搜索栏。在 工具-选项-环境-定位器设置

* 默认情况下，模糊搜索文件名
* 前缀":"(即在搜索栏里输入冒号和空格，然后输入要搜索的内容)，可搜索工程中的类、函数、枚举
