#include <map>
#include <ctime>

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

    std::cout << "unordered_map create cost " << (float(clock()-start))/CLOCKS_PER_SEC << std::endl;

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
    std::cout << "unordered_map find cost " << (float(clock()-start))/CLOCKS_PER_SEC << std::endl;
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

    std::cout << "std_map create cost " << (float(clock()-start))/CLOCKS_PER_SEC << std::endl;

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
    std::cout << "std_map find cost " << (float(clock()-start))/CLOCKS_PER_SEC << std::endl;
}

/*
unordered_map create cost 0.413932
unordered_map find cost 0.129531
std_map create cost 1.60213
std_map find cost 0.915265
*/

int main()
{
    run_unordered_map_test();
    run_std_map_test();
    return 0;
}
