#include "pbc.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>

#include "readfile.h"

static void
dump(uint8_t *buffer, int sz) {
	int i , j;
	for (i=0;i<sz;i++) {
		printf("%02X ",buffer[i]);
		if (i % 16 == 15) {
			for (j = 0 ;j <16 ;j++) {
				char c = buffer[i/16 * 16+j];
				if (c>=32 && c<127) {
					printf("%c",c);
				} else {
					printf(".");
				}
			}
			printf("\n");
		}
	}

	printf("\n");
}

static void
test_rmessage(struct pbc_env *env, struct pbc_slice *slice) {
	struct pbc_rmessage * m = pbc_rmessage_new(env, "test", slice);
	if (m==NULL) {
		printf("Error : %s",pbc_error(env));
		return;
	}

	int phone_n = pbc_rmessage_size(m, "el");
	int i;


	for (i=0;i<phone_n;i++) {
		struct pbc_rmessage * p = pbc_rmessage_message(m , "el", i);
		printf("\tint16_min[%d] = %d\n",i,pbc_rmessage_integer(p , "int16_min", i ,NULL));
		printf("\tdouble_max[%d] = %f\n",i,pbc_rmessage_real(p, "double_max", i));
		printf("\tstring[%d] = %s\n",i,pbc_rmessage_string(p, "str", i, NULL));
	}

	pbc_rmessage_delete(m);
}

static struct pbc_wmessage *
test_wmessage(struct pbc_env * env)
{
	struct pbc_wmessage * msg = pbc_wmessage_new(env, "test");

	int i;
	for ( i = 0;i<5;i++)
	{
		struct pbc_wmessage * element = pbc_wmessage_message(msg , "el");
		pbc_wmessage_string(element , "str", "abcedf" , -1);
		pbc_wmessage_integer(element, "int8_min", 1*(i+1),0);
		pbc_wmessage_integer(element, "int8_max", 2*(i+1),0);
		pbc_wmessage_integer(element, "uint8_min", 3*(i+1),0);
		pbc_wmessage_integer(element, "uint8_max", 4*(i+1),0);

		pbc_wmessage_integer(element, "int16_min", 5*(i+1),0);
		pbc_wmessage_integer(element, "int16_max", 6*(i+1),0);
		pbc_wmessage_integer(element, "uint16_min", 7*(i+1),0);
		pbc_wmessage_integer(element, "uint16_max", 8*(i+1),0);

		pbc_wmessage_integer(element, "int32_min", 9*(i+1),0);
		pbc_wmessage_integer(element, "int32_max", 10*(i+1),0);
		pbc_wmessage_integer(element, "uint32_min", 11*(i+1),0);
		pbc_wmessage_integer(element, "uint32_max", 12*(i+1),0);

		pbc_wmessage_integer(element, "int64_min", 13*(i+1),0);
		pbc_wmessage_integer(element, "int64_max", 14*(i+1),0);
		pbc_wmessage_integer(element, "uint64_min", 15*(i+1),0);
		pbc_wmessage_integer(element, "uint64_max", 16*(i+1),0);

		pbc_wmessage_real(element, "double_min", 17*(i+1));
		pbc_wmessage_real(element, "double_max", 18*(i+1));
	}

	return msg;
}

int
main()
{
	struct pbc_slice slice;
	read_file("performance.pb", &slice);
	if (slice.buffer == NULL)
		return 1;
	struct pbc_env * env = pbc_new();
	int r = pbc_register(env, &slice);
	if (r) {
		printf("Error : %s", pbc_error(env));
		return 1;
	}

	free(slice.buffer);

	clock_t start = clock();
	int i;
	for ( i = 0;i < 1000000;i ++ )
	{
		struct pbc_wmessage *msg = test_wmessage(env);

		pbc_wmessage_buffer(msg, &slice);

		//dump(slice.buffer, slice.len);

		//test_rmessage(env, &slice);

		//if ( i == 100000 - 1) test_rmessage(env, &slice);
		pbc_wmessage_delete(msg);
	}

	clock_t t = clock() - start;
	printf( "time cost %f\n", ((float)t)/CLOCKS_PER_SEC );

	pbc_delete(env);

	return 0;
}

// cd build && gcc -O2 -fPIC -Wall -I.. -L. -o performance ../test/performance.c -lpbc
// 100000 为 time cost 0.390000
// 1000000 为 time cost time cost 4.100000

// 把此文件放到pbc的test目录，然后在pbc目录make即可


// http://blog.codingnow.com/2014/07/sproto.html

// 前面 mingw32 生成的代码在我的系统上有很大的性能问题，所以我用 mingw64 重新编译测试了一次：

// 编码 1M 次	解码 1M 次	体积
// sproto	2.15s	7.84s	83 bytes
// sproto (nopack)	1.58s	6.93s	130 bytes
// pbc-lua	6.94s	16.9s	69 bytes
// lua-cjson	4.92s	8.30s	183 bytes

// pbc与protobuf对比:
// http://blog.codingnow.com/2011/12/pbc_lua_binding.html