# 目录结构

```
root
	engine C++引擎目录
		deps 依赖的第三方库目录
		package 依赖的第三方库特定版本安装包
		src 引擎的C++源码目录
		vcpkg_win_x64 配置好的vcpkg包管理目录，提供引擎在win下编译的依赖
	project 工程配置、项目管理文档目录
	rawresource 原始配置资源
	server 服务器运行目录
		bin 编译好的引擎二进制文件
		c_module 依赖的c库
		certs 证书目录
		config 业务配置文件目录
		pb protobuf相关文件目录
		setting 服务器设置目录（端口、数据库等等）
		src 服务器源码目录
		var 服务器运行的工作目录，包含日志、编译引擎的临时文件等等
	shell 服务器控制脚本目录
	tools 工具目录
```