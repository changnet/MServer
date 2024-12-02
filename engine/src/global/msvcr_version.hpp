#pragma once

// https://dev.to/yumetodo/list-of-mscver-and-mscfullver-8nd

// 当前编译器的版本，可以直接执行cl.exe来查看
// 比如 "D:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.40.33807/bin/Hostx64/x64/cl.exe"
// 用camke的话，编译器所在的路径可以直接从cmake日志查找
// 名字版本，打开VS的帮助/关于可以查看

#if _MSC_VER == 1940    // _MSC_FULL_VER = 194033813
    #define __VERSION__ "Visual Studio 2022 version 17.10.5"
#elif _MSC_VER == 1939	// _MSC_FULL_VER = 193933521
    #define __VERSION__ "Visual Studio 2022 version 17.9.2"
#elif _MSC_VER == 1938	// _MSC_FULL_VER = 193833133
    #define __VERSION__ "Visual Studio 2022 version 17.8.3"
#elif _MSC_VER == 1937	// _MSC_FULL_VER = 193732822
    #define __VERSION__ "Visual Studio 2022 version 17.7.0"
#elif _MSC_VER == 1936	// _MSC_FULL_VER = 193632532
    #define __VERSION__ "Visual Studio 2022 version 17.6.2"
#elif _MSC_VER == 1935	// _MSC_FULL_VER = 193532217
    #define __VERSION__ "Visual Studio 2022 version 17.5.4"
#elif _MSC_VER == 1934	// _MSC_FULL_VER = 193431944
    #define __VERSION__ "Visual Studio 2022 version 17.4.9"
#elif _MSC_VER == 1933 //_MSC_FULL_VER = 193331630
    #define __VERSION__ "Visual Studio 2022 version 17.3.4"
#elif _MSC_VER == 1932 //_MSC_FULL_VER = 193231329
    #define __VERSION__ "Visual Studio 2022 version 17.2.2"
#elif _MSC_VER == 1930 //_MSC_FULL_VER = 193030706
    #define __VERSION__ "Visual Studio 2022 version 17.0.2"
#elif _MSC_VER == 1930 //_MSC_FULL_VER = 193030705
    #define __VERSION__ "Visual Studio 2022 version 17.0.1"
#elif _MSC_VER == 1929 //_MSC_FULL_VER = 192930133
    #define __VERSION__ "Visual Studio 2019 version 16.11.2"
#elif _MSC_VER == 1928 //_MSC_FULL_VER = 192829913
    #define __VERSION__ "Visual Studio 2019 version 16.9.2"
#elif _MSC_VER == 1928 //_MSC_FULL_VER = 192829334
    #define __VERSION__ "Visual Studio 2019 version 16.8.2"
#elif _MSC_VER == 1928 //_MSC_FULL_VER = 192829333
    #define __VERSION__ "Visual Studio 2019 version 16.8.1"
#elif _MSC_VER == 1927 //_MSC_FULL_VER = 192729112
    #define __VERSION__ "Visual Studio 2019 version 16.7"
#elif _MSC_VER == 1926 //_MSC_FULL_VER = 192628806
    #define __VERSION__ "Visual Studio 2019 version 16.6.2"
#elif _MSC_VER == 1925 //_MSC_FULL_VER = 192528611
    #define __VERSION__ "Visual Studio 2019 version 16.5.1"
#elif _MSC_VER == 1924 //_MSC_FULL_VER = 192428314
    #define __VERSION__ "Visual Studio 2019 version 16.4.0"
#elif _MSC_VER == 1923 //_MSC_FULL_VER = 192328105
    #define __VERSION__ "Visual Studio 2019 version 16.3.2"
#elif _MSC_VER == 1922 //_MSC_FULL_VER = 192227905
    #define __VERSION__ "Visual Studio 2019 version 16.2.3"
#elif _MSC_VER == 1921 //_MSC_FULL_VER = 192127702
    #define __VERSION__ "Visual Studio 2019 version 16.1.2"
#elif _MSC_VER == 1920 //_MSC_FULL_VER = 192027508
    #define __VERSION__ "Visual Studio 2019 version 16.0.0"
#elif _MSC_VER == 1916 //_MSC_FULL_VER = 191627030
    #define __VERSION__ "Visual Studio 2017 version 15.9.11"
#elif _MSC_VER == 1916 //_MSC_FULL_VER = 191627027
    #define __VERSION__ "Visual Studio 2017 version 15.9.7"
#elif _MSC_VER == 1916 //_MSC_FULL_VER = 191627026
    #define __VERSION__ "Visual Studio 2017 version 15.9.5"
#elif _MSC_VER == 1916 //_MSC_FULL_VER = 191627025
    #define __VERSION__ "Visual Studio 2017 version 15.9.4"
#elif _MSC_VER == 1916 //_MSC_FULL_VER = 191627023
    #define __VERSION__ "Visual Studio 2017 version 15.9.1"
#elif _MSC_VER == 1916 //_MSC_FULL_VER =          
    #define __VERSION__ "Visual Studio 2017 version 15.9.0"
#elif _MSC_VER == 1915 //_MSC_FULL_VER =          
    #define __VERSION__ "Visual Studio 2017 version 15.8.0"
#elif _MSC_VER == 1914 //_MSC_FULL_VER = 191426433
    #define __VERSION__ "Visual Studio 2017 version 15.7.5"
#elif _MSC_VER == 1914 //_MSC_FULL_VER = 191426430
    #define __VERSION__ "Visual Studio 2017 version 15.7.3"
#elif _MSC_VER == 1914 //_MSC_FULL_VER = 191426429
    #define __VERSION__ "Visual Studio 2017 version 15.7.2"
#elif _MSC_VER == 1914 //_MSC_FULL_VER = 191426428
    #define __VERSION__ "Visual Studio 2017 version 15.7.1"
#elif _MSC_VER == 1913 //_MSC_FULL_VER = 191326132
    #define __VERSION__ "Visual Studio 2017 version 15.6.7"
#elif _MSC_VER == 1913 //_MSC_FULL_VER = 191326131
    #define __VERSION__ "Visual Studio 2017 version 15.6.6"
#elif _MSC_VER == 1913 //_MSC_FULL_VER = 191326129
    #define __VERSION__ "Visual Studio 2017 version 15.6.4"
#elif _MSC_VER == 1913 //_MSC_FULL_VER = 191326129
    #define __VERSION__ "Visual Studio 2017 version 15.6.3"
#elif _MSC_VER == 1913 //_MSC_FULL_VER = 191326128
    #define __VERSION__ "Visual Studio 2017 version 15.6.2"
#elif _MSC_VER == 1913 //_MSC_FULL_VER = 191326128
    #define __VERSION__ "Visual Studio 2017 version 15.6.1"
#elif _MSC_VER == 1913 //_MSC_FULL_VER = 191326128
    #define __VERSION__ "Visual Studio 2017 version 15.6.0"
#elif _MSC_VER == 1912 //_MSC_FULL_VER = 191225835
    #define __VERSION__ "Visual Studio 2017 version 15.5.7"
#elif _MSC_VER == 1912 //_MSC_FULL_VER = 191225835
    #define __VERSION__ "Visual Studio 2017 version 15.5.6"
#elif _MSC_VER == 1912 //_MSC_FULL_VER = 191225834
    #define __VERSION__ "Visual Studio 2017 version 15.5.4"
#elif _MSC_VER == 1912 //_MSC_FULL_VER = 191225834
    #define __VERSION__ "Visual Studio 2017 version 15.5.3"
#elif _MSC_VER == 1912 //_MSC_FULL_VER = 191225831
    #define __VERSION__ "Visual Studio 2017 version 15.5.2"
#elif _MSC_VER == 1911 //_MSC_FULL_VER = 191125547
    #define __VERSION__ "Visual Studio 2017 version 15.4.5"
#elif _MSC_VER == 1911 //_MSC_FULL_VER = 191125542
    #define __VERSION__ "Visual Studio 2017 version 15.4.4"
#elif _MSC_VER == 1911 //_MSC_FULL_VER = 191125507
    #define __VERSION__ "Visual Studio 2017 version 15.3.3"
#elif _MSC_VER == 1910 //_MSC_FULL_VER = 191025017
    #define __VERSION__ "Visual Studio 2017 version 15.2"
#elif _MSC_VER == 1910 //_MSC_FULL_VER = 191025017
    #define __VERSION__ "Visual Studio 2017 version 15.1"
#elif _MSC_VER == 1910 //_MSC_FULL_VER = 191025017
    #define __VERSION__ "Visual Studio 2017 version 15.0"
#elif _MSC_VER == 1900 //_MSC_FULL_VER = 190024210
    #define __VERSION__ "Visual Studio 2015 Update 3 [14.0]"
#elif _MSC_VER == 1900 //_MSC_FULL_VER = 190023918
    #define __VERSION__ "Visual Studio 2015 Update 2 [14.0]"
#elif _MSC_VER == 1900 //_MSC_FULL_VER = 190023506
    #define __VERSION__ "Visual Studio 2015 Update 1 [14.0]"
#elif _MSC_VER == 1900 //_MSC_FULL_VER = 190023026
    #define __VERSION__ "Visual Studio 2015 [14.0]"
#elif _MSC_VER == 1800 //_MSC_FULL_VER = 180021114
    #define __VERSION__ "Visual Studio 2013 November CTP [12.0]"
#elif _MSC_VER == 1800 //_MSC_FULL_VER = 180040629
    #define __VERSION__ "Visual Studio 2013 Update 5 [12.0]"
#elif _MSC_VER == 1800 //_MSC_FULL_VER = 180031101
    #define __VERSION__ "Visual Studio 2013 Update 4 [12.0]"
#elif _MSC_VER == 1800 //_MSC_FULL_VER = 180030723
    #define __VERSION__ "Visual Studio 2013 Update 3 [12.0]"
#elif _MSC_VER == 1800 //_MSC_FULL_VER = 180030501
    #define __VERSION__ "Visual Studio 2013 Update 2 [12.0]"
#elif _MSC_VER == 1800 //_MSC_FULL_VER = 180030324
    #define __VERSION__ "Visual Studio 2013 Update2 RC [12.0]"
#elif _MSC_VER == 1800 //_MSC_FULL_VER = 180021005
    #define __VERSION__ "Visual Studio 2013 Update 1 [12.0]"
#elif _MSC_VER == 1800 //_MSC_FULL_VER = 180021005
    #define __VERSION__ "Visual Studio 2013 [12.0]"
#elif _MSC_VER == 1800 //_MSC_FULL_VER = 180020827
    #define __VERSION__ "Visual Studio 2013 RC [12.0]"
#elif _MSC_VER == 1800 //_MSC_FULL_VER = 180020617
    #define __VERSION__ "Visual Studio 2013 Preview [12.0]"
#elif _MSC_VER == 1700 //_MSC_FULL_VER = 170051025
    #define __VERSION__ "Visual Studio 2012 November CTP [11.0]"
#elif _MSC_VER == 1700 //_MSC_FULL_VER = 170061030
    #define __VERSION__ "Visual Studio 2012 Update 4 [11.0]"
#elif _MSC_VER == 1700 //_MSC_FULL_VER = 170060610
    #define __VERSION__ "Visual Studio 2012 Update 3 [11.0]"
#elif _MSC_VER == 1700 //_MSC_FULL_VER = 170060315
    #define __VERSION__ "Visual Studio 2012 Update 2 [11.0]"
#elif _MSC_VER == 1700 //_MSC_FULL_VER = 170051106
    #define __VERSION__ "Visual Studio 2012 Update 1 [11.0]"
#elif _MSC_VER == 1700 //_MSC_FULL_VER = 170050727
    #define __VERSION__ "Visual Studio 2012 [11.0]"
#elif _MSC_VER == 1600 //_MSC_FULL_VER = 160040219
    #define __VERSION__ "Visual Studio 2010 SP1 [10.0], Visual C++ 2010 SP1 [10.0]"
#elif _MSC_VER == 1600 //_MSC_FULL_VER = 160030319
    #define __VERSION__ "Visual Studio 2010 [10.0], Visual C++ 2010 [10.0]"
#elif _MSC_VER == 1600 //_MSC_FULL_VER = 160021003
    #define __VERSION__ "Visual Studio 2010 Beta 2 [10.0]"
#elif _MSC_VER == 1600 //_MSC_FULL_VER = 160020506
    #define __VERSION__ "Visual Studio 2010 Beta 1 [10.0]"
#elif _MSC_VER == 1500 //_MSC_FULL_VER = 150030729
    #define __VERSION__ "Visual Studio 2008 SP1 [9.0], Visual C++ 2008 SP1 [9.0]"
#elif _MSC_VER == 1500 //_MSC_FULL_VER = 150021022
    #define __VERSION__ "Visual Studio 2008 [9.0], Visual C++ 2008 [9.0]"
#elif _MSC_VER == 1500 //_MSC_FULL_VER = 150020706
    #define __VERSION__ "Visual Studio 2008 Beta 2 [9.0]"
#elif _MSC_VER == 1400 //_MSC_FULL_VER = 140050727
    #define __VERSION__ "Visual Studio 2005 SP1 [8.0], Visual C++ 2005 SP1 [8.0]"
#elif _MSC_VER == 1400 //_MSC_FULL_VER = 140050320
    #define __VERSION__ "Visual Studio 2005 [8.0],Visual C++ 2005 [8.0]"
#elif _MSC_VER == 1400 //_MSC_FULL_VER = 140050215
    #define __VERSION__ "Visual Studio 2005 Beta 2 [8.0]"
#elif _MSC_VER == 1400 //_MSC_FULL_VER = 140040607
    #define __VERSION__ "Visual Studio 2005 Beta 1 [8.0]"
#elif _MSC_VER == 1400 //_MSC_FULL_VER = 140040310
    #define __VERSION__ "Windows Server 2003 SP1 DDK (for AMD64)"
#elif _MSC_VER == 1310 //_MSC_FULL_VER = 13106030 
    #define __VERSION__ "Visual Studio .NET 2003 SP1 [7.1], Visual C++ .NET 2003 SP1 [7.1]"
#elif _MSC_VER == 1310 //_MSC_FULL_VER = 13104035 
    #define __VERSION__ "Windows Server 2003 SP1 DDK"
#elif _MSC_VER == 1310 //_MSC_FULL_VER = 13103077 
    #define __VERSION__ "Visual Studio .NET 2003 [7.1], Visual C++ .NET 2003 [7.1]"
#elif _MSC_VER == 1310 //_MSC_FULL_VER = 13103052 
    #define __VERSION__ "Visual Studio Toolkit 2003 [7.1]"
#elif _MSC_VER == 1310 //_MSC_FULL_VER = 13102292 
    #define __VERSION__ "Visual Studio .NET 2003 Beta [7.1]"
#elif _MSC_VER == 1310 //_MSC_FULL_VER = 13102179 
    #define __VERSION__ "Windows Server 2003 DDK"
#elif _MSC_VER == 1300 //_MSC_FULL_VER = 13009466 
    #define __VERSION__ "Visual Studio .NET 2002 [7.0], Visual C++ .NET 2002 [7.0]"
#elif _MSC_VER == 1300 //_MSC_FULL_VER = 13009176 
    #define __VERSION__ "Windows XP SP1 DDK"
#elif _MSC_VER == 1200 //_MSC_FULL_VER = 12008804 
    #define __VERSION__ "Visual Studio 6.0 SP6, Visual C++ 6.0 SP6"
#elif _MSC_VER == 1200 //_MSC_FULL_VER = 12008804 
    #define __VERSION__ "Visual Studio 6.0 SP5, Visual C++ 6.0 SP5"
#elif _MSC_VER == 1100 //_MSC_FULL_VER =          
    #define __VERSION__ "Visual Studio 97 [5.0], Visual C++ 5.0"
#elif _MSC_VER == 1020 //_MSC_FULL_VER =          
    #define __VERSION__ "Visual C++ 4.2"
#elif _MSC_VER == 1010 //_MSC_FULL_VER =          
    #define __VERSION__ "Visual C++ 4.1"
#elif _MSC_VER == 1000 //_MSC_FULL_VER =          
    #define __VERSION__ "Visual C++ 4.0"
#elif _MSC_VER == 900  //_MSC_FULL_VER =          
    #define __VERSION__ "Visual C++ 2.0"
#elif _MSC_VER == 800  //_MSC_FULL_VER =          
    #define __VERSION__ "Visual C++ 1.0"
#elif _MSC_VER == 700  //_MSC_FULL_VER =          
    #define __VERSION__ "Microsoft C/C++"
#elif _MSC_VER == 600  //_MSC_FULL_VER =          
    #define __VERSION__ "Microsoft C 6.0"
#else
    #define __VERSION__ XSTR(_MSC_FULL_VER)
#endif
