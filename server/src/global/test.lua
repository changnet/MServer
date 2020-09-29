-- simple test facility

-- describe a test case
local Describe = {}

-- describe constructor
-- @param title a custom test case tile
-- @param func test callback function
function Describe:__init(title, func)
end

rawset(Describe, "__index", Describe)
setmetatable(Describe, {
    __call = function(self, ...)
        local ins = setmetatable({}, Describe)
        ins:__init(...)
        return ins;
    end
})

-- a specification or test-case with the given `title` and callback `fn`
local It = {}

-- describe constructor
-- @param title a custom test case tile
-- @param func test callback function
function It:__init(title, func)
end

rawset(It, "__index", It)
setmetatable(It, {
    __call = function(self, ...)
        local ins = setmetatable({}, It)
        ins:__init(...)
        return ins;
    end
})

-- /////////////////// GLOBAL test function ////////////////////////////////////

function t_equal()
end

function t_assert()
end

function t_describe(title, func)
end

function t_it(title, func)
end

function t_wait(timeout)
end

function t_done()
end

function t_setup(params)
    -- timer func
end
