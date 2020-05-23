### Qt Creator project file

https://www.qt.io/offline-installers

我主要是在linux下用Visual Studio Code写这个框架，所以一般是不用IDE的。
如果一定要用IDE，推荐跨平台的Qt Creator。

由于我不用IDE，这个Qt工程可能会遗漏一些新增的文件，需要自己去添加。

这个工程只是用来方便阅读分析代码的。编译、调试还是用Makefile和gdb。

### no suitable kit found
如果Qt Creator打开工程提示：no suitable kit found，是因为没有安装qmake，无法解析工程文件

apt install qt5-default，然后在Qt Creator tools->Options->Qt Versions->add，最后设置好 构建套件(Kits)中的qmake版本即可

### clang-format
在Qt Creator的 帮助->插件里，新版本有clang-format支持。如果不能用（https://bugs.launchpad.net/ubuntu/+source/qtcreator/+bug/1876436），则在Beautifier这个插件里。
配置选择File即可，.clang-format配置文件必须在engine/src根目录，否则格式化不生效，也不会有什么错误提示

### 添加已有目录
1. 在工程上右键，选择 添加已有目录
2. 选择对应的目录，写上过滤的正则，点击开始解析
3. 勾选上解析到的文件即可

### 添加库
只有源码，在工程中需要编译的库称为内部库。直接使用特定位置，已编译好的库为外部库。
由于我们deps目录都是预先编译好的，一般不用再次编译。因此deps目录下的库都是外部库。

1. 先编译好deps下的库
2. 在工程上右键，选择 添加库，选择外部库
3. 打开对应的库目录(即编译好的so文件或者a文件所在目录)，include目录
4. 选linux平台，动态或者静态链接即可

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
