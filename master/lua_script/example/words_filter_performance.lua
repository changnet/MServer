local Acism = require "Acism"

local acism = Acism()

acism:load_from_file( "lua_script/config/illeagal_worlds.txt",false )

-- scan返回匹配字符串尾位置，一般一个中文字符占3
print( "acism worlds filter",acism:scan("this is nothing") )
print( "acism worlds filter",acism:scan("this is something,找到了18Dy") )
print( "acism worlds filter",acism:scan("出售枪支是非法的") )
