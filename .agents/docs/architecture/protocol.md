# 定义协议
1. 先在server\pb\目录中的proto文件中定义结构，一些公用的结构在comm.proto中
2. 协议在src\modules\protocol\protocol.lua中定义，不需要定义协议id
3. 进入shell目录，执行auto_proto.bat会自动生成协议id并导出pb文件
