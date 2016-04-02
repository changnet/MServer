# lua_rapidxml

[![Build Status](https://travis-ci.org/changnet/lua_rapidxml.svg?branch=master)](https://travis-ci.org/changnet/lua_rapidxml)

A lua xml encode/decode c++ module base on rapidxml(v1.13)  
See more about rapidxml at http://rapidxml.sourceforge.net/

Installation
------------

 * Make sure lua develop environment already installed
 * Run 'git clone https://github.com/changnet/lua_rapidxml.git'
 * Run 'cd lua_rapidxml'
 * Run 'make'
 * Run 'make test' to test
 * Copy lua_rapidxml.so to your lua project's c module directory

or embed to your project

Api
-----

```lua
encode( tb,pretty )
encode_to_file( tb,file,pretty )

decode( str )
decode_from_file( file )
```

Conversion Rules
----------------
 * xml document will be converted into a array(a lua table,key is 1 ... n)
 * every xml element will be converted into a lua table
   1. name is stored at key "name"
   2. attribute is stored at key "attribute" as a lua table
   3. value is stored at key "value" as a string or a array(if it's more than one value)
 * namespace was treated like attribute
 * pre-defined entity references were treated like strings

Example
--------
see "test.lua" for more.  

```xml
<?xml version="1.0" encoding="utf-8"?>
<module xmlns:n = "lua_rapidxml">
    <n:name>lua_rapidxml</name>
</module>
```

```lua
{
    1 = table: 0x1870590
    {
        value = table: 0x1867ae0
        {
            1 = table: 0x1867b20
            {
                name = n:name
                value = lua_rapidxml
            }
        }
        name = module
        attribute = table: 0x1867c10
        {
            xmlns:n = lua_rapidxml
        }
    }
}
```
