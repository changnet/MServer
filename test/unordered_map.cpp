#include <map>
#include <ctime>
#include <cstdio>
#include <vector>
#include <cstdlib>     /* srand, rand */

#include <cstring>
#include <iostream>

#include "../master/cpp_src/global/types.h"

#if(__cplusplus >= 201103L)
# include <unordered_map>
#else
# include <tr1/unordered_map>
namespace std
{
    using std::tr1::unordered_map;
}
#endif

#include <iostream>

#define M_TS    1000
#define N_TS    1000

typedef std::pair<unsigned short,unsigned short> pair_key_t;
struct pair_hash
{
    unsigned int operator () (const pair_key_t& pk) const
    {
        return (0xffff0000 & (pk.first << 16)) | (0x0000ffff & pk.second);
    }
};

struct pair_equal
{
    bool operator () (const pair_key_t& a, const pair_key_t& b) const
    {
        return a.first == b.first && a.second == b.second;
    }
};

struct pair_less
{
    bool operator () (const pair_key_t& a, const pair_key_t& b) const
    {
        return (a.first < b.first) || (a.first == b.first && a.second < b.second);
    }
};

typedef std::map< pair_key_t,unsigned int,pair_less > std_map_t;
typedef std::unordered_map< pair_key_t,unsigned int,pair_hash,pair_equal > unordered_map_t;

void run_unordered_map_test()
{
    unordered_map_t _protocol;

    clock_t start = clock();
    for ( unsigned short m = 0;m < M_TS;m ++ )
        for ( unsigned short n = 0;n < N_TS;n ++ )
        {
            unsigned int result = (0xffff0000 & (m << 16)) | (0x0000ffff & n);
            _protocol.insert( std::make_pair(std::make_pair(m,n),result) );
        }

    std::cout << "unordered_map create cost "
        << (float(clock()-start))/CLOCKS_PER_SEC << std::endl;

    start = clock();
    for ( unsigned short m = 0;m < M_TS;m ++ )
        for ( unsigned short n = 0;n < N_TS;n ++ )
        {
            unordered_map_t::iterator itr = _protocol.find( std::make_pair(m,n) );
            if ( itr == _protocol.end() )
            {
                std::cout << "unordered_map error" << std::endl;
                return;
            }
        }
    std::cout << "unordered_map find cost "
        << (float(clock()-start))/CLOCKS_PER_SEC << std::endl;
}

void run_std_map_test()
{
    std_map_t _protocol;

    clock_t start = clock();
    for ( unsigned short m = 0;m < M_TS;m ++ )
        for ( unsigned short n = 0;n < N_TS;n ++ )
        {
            unsigned int result = (0xffff0000 & (m << 16)) | (0x0000ffff & n);
            _protocol.insert( std::make_pair(std::make_pair(m,n),result) );
        }

    std::cout << "std_map create cost "
        << (float(clock()-start))/CLOCKS_PER_SEC << std::endl;

    start = clock();
    for ( unsigned short m = 0;m < M_TS;m ++ )
        for ( unsigned short n = 0;n < N_TS;n ++ )
        {
            std_map_t::iterator itr = _protocol.find( std::make_pair(m,n) );
            if ( itr == _protocol.end() )
            {
                std::cout << "std_map error" << std::endl;
                return;
            }
        }
    std::cout << "std_map find cost "
        << (float(clock()-start))/CLOCKS_PER_SEC << std::endl;
}

void create_random_key(std::vector<std::string> &vt)
{
    srand (time(NULL));

    for (int idx = 0;idx < 100000;idx ++)
    {
        int ikey = rand();
        int ikey2 = rand();

        char skey[64];
        sprintf(skey,"%X%X",ikey,ikey2);

        vt.push_back(skey);
    }
}

void test_unorder_string(const std::vector<std::string> &vt)
{
    std::unordered_map<std::string,int> test_map;

    clock_t start = clock();
    for (int idx = 0;idx < vt.size();idx ++)
    {
        test_map[ vt[idx].c_str() ] = idx;
    }
    std::cout << "unorder_map std::string create cost "
        << (float(clock()-start))/CLOCKS_PER_SEC << std::endl;

    start = clock();
    for (int idx = 0;idx < vt.size()/2;idx ++)
    {
        if (test_map.find(vt[idx].c_str()) == test_map.end())
        {
            std::cout << "unorder_map std::string find fail" << std::endl;
            return;
        }
    }

    std::cout << "unorder_map std::string find cost "
        << (float(clock()-start))/CLOCKS_PER_SEC << std::endl;
}


void test_unorder_char(const std::vector<std::string> &vt)
{
    std::unordered_map<const c_string,int,hash_c_string,equal_c_string> test_map;

    clock_t start = clock();
    for (int idx = 0;idx < vt.size();idx ++)
    {
        test_map[ vt[idx].c_str() ] = idx;
    }
    std::cout << "unorder_map char create cost "
        << (float(clock()-start))/CLOCKS_PER_SEC << std::endl;

    start = clock();
    for (int idx = 0;idx < vt.size()/2;idx ++)
    {
        if (test_map.find(vt[idx].c_str()) == test_map.end())
        {
            std::cout << "unorder_map char find fail" << std::endl;
            return;
        }
    }

    std::cout << "unorder_map char find cost "
        << (float(clock()-start))/CLOCKS_PER_SEC << std::endl;
}

void test_stdmap_char(const std::vector<std::string> &vt)
{
    std::map<const char *,int,cmp_const_char> test_map;

    clock_t start = clock();
    for (int idx = 0;idx < vt.size();idx ++)
    {
        test_map[ vt[idx].c_str() ] = idx;
    }
    std::cout << "std_map char create cost "
        << (float(clock()-start))/CLOCKS_PER_SEC << std::endl;

    start = clock();
    for (int idx = 0;idx < vt.size()/2;idx ++)
    {
        if (test_map.find(vt[idx].c_str()) == test_map.end())
        {
            std::cout << "std_map char find fail" << std::endl;
            return;
        }
    }

    std::cout << "std_map char find cost "
        << (float(clock()-start))/CLOCKS_PER_SEC << std::endl;
}


void test_stdmap_string(const std::vector<std::string> &vt)
{
    std::map<std::string,int> test_map;

    clock_t start = clock();
    for (int idx = 0;idx < vt.size();idx ++)
    {
        test_map[ vt[idx] ] = idx;
    }
    std::cout << "std_map string create cost "
        << (float(clock()-start))/CLOCKS_PER_SEC << std::endl;

    start = clock();
    for (int idx = 0;idx < vt.size()/2;idx ++)
    {
        if (test_map.find(vt[idx]) == test_map.end())
        {
            std::cout << "std_map char find fail" << std::endl;
            return;
        }
    }

    std::cout << "std_map string find cost "
        << (float(clock()-start))/CLOCKS_PER_SEC << std::endl;
}


/*

unordered_map create cost 0.08
unordered_map find cost 0.01
std_map create cost 0.28
std_map find cost 0.13

key为10000000时：
std_map char create cost 31.73
std_map char find cost 15.69
std_map string create cost 56.44
std_map string find cost 28.48
unorder_map char create cost 11.61
unorder_map char find cost 1.17
unorder_map std::string create cost 11.75
unorder_map std::string find cost 2.13

key为100000时
std_map char create cost 0.09
std_map char find cost 0.04
std_map string create cost 0.15
std_map string find cost 0.08
unorder_map char create cost 0.03
unorder_map char find cost 0.01
unorder_map std::string create cost 0.06
unorder_map std::string find cost 0.02

*/

// valgrind --tool=callgrind --instr-atstart=no ./unoder_map
int main()
{
    run_unordered_map_test();
    run_std_map_test();

    std::vector<std::string> vt;

    create_random_key(vt);

    test_stdmap_char(vt);
    test_stdmap_string(vt);
    int idx = 0; // wait valgrind:callgrind_control -i on
    //std::cin >> idx;
    test_unorder_char(vt);
    test_unorder_string(vt);
    //std::cin >> idx;
    return 0;
}
