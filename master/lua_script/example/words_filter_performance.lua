local Acism = require "Acism"

local acism = Acism()

acism:load_from_file( "lua_script/config/illeagal_worlds.txt",false )

-- scan返回匹配字符串尾位置，一般一个中文字符占3
print( "acism worlds filter",acism:scan("this is nothing") )
print( "acism worlds filter",acism:scan("this is something,找到了18Dy") )
print( "acism worlds filter",acism:scan("出售枪支是非法的") )

-- 改小缓冲区测试,做一些边缘测试
print( "acism worlds replace",acism:replace("出售枪支是非法的","***") )
print( "acism worlds replace",acism:replace("18dy18Dy18dy18dy","***") )
print( "acism worlds replace",acism:replace("出售枪支是非法的","************************************************") )
print( "acism worlds replace",acism:replace("","***") )
print( "acism worlds replace",acism:replace("出售枪支是非法的","~") )
print( "acism worlds replace",acism:replace("出售枪支是非法的","") )
print( "acism worlds replace",acism:replace("","") )
print( "acism worlds replace",acism:replace("这句话是不应该发生内存拷贝的","***") )
