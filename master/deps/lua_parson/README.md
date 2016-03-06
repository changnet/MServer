lua_parson
==========

A lua json encode/decode c module base on parson.  
See more about parson at https://github.com/kgabis/parson

Installation
------------

 * Make sure lua develop environment already installed
 * Run 'git clone https://github.com/changnet/lua_parson.git'
 * Run 'cd lua_parson'
 * Run 'make'
 * Run 'lua test.lua' to test
 * Copy lua_parson.so to your lua project c module directory

Api
-----

```lua
encode( tb,pretty )
encode_to_file( tb,file,pretty )

decode( str )
decode_from_file( file )
``` 

Array Object
------------

 * If a table has only integer key,we consider it an array.otherwise is's an object.
 * If a table's metatable has field '__array' and it's value is true,consider the table an array
 * If a table's metatable has field '__array' and it's value is false,consider the table an object
 * Empty table is an object

Note:
 * If a array has illeage index(string or number key),it's key will be replace with index 1..n when encode
 * A sparse array will be filled with 'null' when encode

Example
-------

See 'test.lua'  

```json
{"empty_object":{},"employees":[{"firstName":"Bill","lastName":"Gates"},{"firstName":"George","lastName":"Bush"},{"firstName":"Thomas","lastName":"Carter"}],"force_object":{"1":"USA","2":"UK","3":"CH"},"force_array":["987654321","123456789"],"sparse":[null,null,null,null,null,null,null,null,null,"number ten"],"empty_array":[]}
```

```lua
{
    empty_object = 
    {
    }
    employees = 
    {
        [1] = 
        {
            firstName = "Bill"
            lastName = "Gates"
        }
        [2] = 
        {
            firstName = "George"
            lastName = "Bush"
        }
        [3] = 
        {
            firstName = "Thomas"
            lastName = "Carter"
        }
    }
    empty_array = 
    {
    }
    force_array = 
    {
        [1] = 987654321
        [2] = 123456789
    }
    sparse = 
    {
        [10] = "number ten"
    }
    force_object = 
    {
        ["3"] = "CH"
        ["2"] = "UK"
        ["1"] = "USA"
    }
}
```


