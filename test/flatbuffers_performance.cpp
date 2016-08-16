#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"

#include <ctime>
#include <cstring>
#include <iostream>

/* fbs文件规则 https://google.github.io/flatbuffers/md__schemas.html
 * Any table can be a root in a buffer. The root type is mostly for use by JSON,
 * which doesn't specify any types, so we need to know where to start
 * 编码原理:https://google.github.io/flatbuffers/md__internals.html
 *
 * 内存分布:http://www.race604.com/flatbuffers-intro/
 */

flatbuffers::uoffset_t encode_object( flatbuffers::FlatBufferBuilder &fbb,const reflection::Object *obj )
{
	const uint8_t  bool_val   = true;
    const int8_t   char_val   = -127;  // 0x81
    const uint8_t  uchar_val  = 0xFF;
    const int16_t  short_val  = -32222; // 0x8222;
    const uint16_t ushort_val = 0xFEEE;
    const int32_t  int_val    = 0x83333333;
    const uint32_t uint_val   = 0xFDDDDDDD;
    const int64_t  long_val   = 0x8444444444444444LL;
    const uint64_t ulong_val  = 0xFCCCCCCCCCCCCCCCULL;
    const float    float_val  = 3.14159f;
    const double   double_val = 3.14159265359;

    // const flatbuffers::Vector<flatbuffers::Offset<Field>> *fields()
	const auto *fields = obj->fields();

	flatbuffers::uoffset_t start = fbb.StartTable();

	flatbuffers::Vector< flatbuffers::Offset<reflection::Field> >::const_iterator itr = fields->begin();
    while ( itr != fields->end() )
    {
    	// 从lua转时，直接将name当key push取值，如果取不到而field.is_require则报错
        const reflection::Field *field = *itr;
        uint16_t off = field->offset();
        switch ( field->type()->base_type() )
        {
        //case reflection::UType:
		case reflection::Bool:   fbb.AddElement<uint8_t >(off, bool_val,   0); break;
		case reflection::UByte:  fbb.AddElement<int8_t  >(off, char_val,   0); break;
		case reflection::Byte:   fbb.AddElement<uint8_t >(off, uchar_val,  0); break;
		case reflection::Short:  fbb.AddElement<int16_t >(off, short_val,  0); break;
		case reflection::UShort: fbb.AddElement<uint16_t>(off, ushort_val, 0); break;
		case reflection::Int:    fbb.AddElement<int32_t >(off, int_val,    0); break;
		case reflection::UInt:   fbb.AddElement<uint32_t>(off, uint_val,   0); break;
		case reflection::Long:   fbb.AddElement<int64_t >(off, long_val,   0); break;
		case reflection::ULong:  fbb.AddElement<uint64_t>(off, ulong_val,  0); break;
		case reflection::Float:  fbb.AddElement<float   >(off, float_val,  0); break;
		case reflection::Double: fbb.AddElement<double  >(off, double_val, 0); break;
        }

        ++itr;
    }

    return fbb.EndTable(start, fields->size());
}

flatbuffers::uoffset_t encode_vector( flatbuffers::FlatBufferBuilder &fbb,const reflection::Object *obj )
{
	std::vector< flatbuffers::Offset<void> > vOffsets;
    for (int i = 0;i < 5;i ++)
    {
    	vOffsets.push_back( encode_object( fbb,obj ) );
    }

    return fbb.CreateVector(vOffsets).o;
}

flatbuffers::uoffset_t encode_test( flatbuffers::FlatBufferBuilder &fbb,const reflection::Schema *schema )
{
	const reflection::Object* pObj = schema->objects()->LookupByKey("test");
	if ( !pObj )
	{
	  std::cerr << "no object found" << std::endl;
	  return 2;
	}

	const auto *test_fields = pObj->fields();
	const auto *element_field = test_fields->LookupByKey("el");

	const reflection::Object *obj = schema->objects()->Get(element_field->type()->index());

	flatbuffers::uoffset_t off = encode_vector(fbb,obj);
	flatbuffers::uoffset_t start = fbb.StartTable();
	// for ( field)
	fbb.AddOffset(element_field->offset(),flatbuffers::Offset<void>( off ));

	return fbb.EndTable(start, test_fields->size());
}

void decode_table( const reflection::Object* obj,const flatbuffers::Table& table )
{
	const auto *fields = obj->fields();
	flatbuffers::Vector< flatbuffers::Offset<reflection::Field> >::const_iterator itr = fields->begin();
    while ( itr != fields->end() )
    {
    	// 从lua转时，直接将name当key push取值，如果取不到而field.is_require则报错
        const reflection::Field *field = *itr;
        uint16_t off = field->offset();
        switch ( field->type()->base_type() )
        {
        // 有些field是optional的，需要verify来判断是否存在 TableDecoder::VerifyFieldOfTable
        //case reflection::UType:
		case reflection::Bool: break;
		case reflection::UByte: break;
		case reflection::Byte: break;
		case reflection::Short:
			{
				int16_t val = flatbuffers::GetFieldI<int16_t>(table, *field);
				std::cout << "int16 is " << val << std::endl;
			}break;
		case reflection::UShort: break;
		case reflection::Int:
			{
				int32_t val = flatbuffers::GetFieldI<int32_t>(table, *field);
				std::cout << "int is " << val << std::endl;
			}break;
		case reflection::UInt: break;
		case reflection::Long: break;
		case reflection::ULong:
			{
				uint64_t val = flatbuffers::GetFieldI<uint64_t>(table, *field);
				std::cout << "uLong is " << val << std::endl;
			}break;
		case reflection::Float:break;
		case reflection::Double:break;
        }

        ++itr;
    }

    std::cout << "int should be " << (int32_t)0x83333333 << std::endl;
    std::cout << "ulong should be " << 0xFCCCCCCCCCCCCCCCULL << std::endl;
}

void decode_test( const char *flatbuf,int len,const reflection::Schema *schema )
{
	const reflection::Object* pObj = schema->objects()->LookupByKey("test");
	if ( !pObj )
	{
	  std::cerr << "no object found" << std::endl;
	  return;
	}

	const auto *test_fields = pObj->fields();
	const auto *element_field = test_fields->LookupByKey("el");

	const void* pRoot = flatbuffers::GetRoot<void>(flatbuf);
	const flatbuffers::Table &table = *reinterpret_cast<const flatbuffers::Table*>(pRoot);

	//如果已经知道元素类型，才能用GetFieldV
	const flatbuffers::VectorOfAny *vec = table.GetPointer< const flatbuffers::VectorOfAny* >( element_field->offset() );

	for ( int i = 0;i < vec->size();i ++ )
	{
		const reflection::Object *obj = schema->objects()->Get(element_field->type()->index());
		const auto *tb = flatbuffers::GetAnyVectorElemPointer<
			const flatbuffers::Table>(vec, i);
		decode_table( obj,*tb );
	}

}

int main()
{
	std::string bfbsfile;
	if ( !flatbuffers::LoadFile("performance.bfbs", true, &bfbsfile) )
	{
	   std::cerr << "load bfbs fail" << std::endl;
		return 1;
	}

	flatbuffers::Verifier encode_verifier(
    	reinterpret_cast<const uint8_t *>(bfbsfile.c_str()), bfbsfile.length());
  	assert( reflection::VerifySchemaBuffer(encode_verifier) );

	const auto *schema = reflection::GetSchema(bfbsfile.c_str());
	
	const int max = 10;
	clock_t start = clock();
	for ( int i = 0;i < max;i ++ )
	{
		flatbuffers::FlatBufferBuilder fbb;
		flatbuffers::uoffset_t offset = encode_test( fbb,schema );
		fbb.Finish( flatbuffers::Offset<void>(offset) );

		if ( i == max -1 )
		{
			const char *buff = reinterpret_cast<const char*>( fbb.GetBufferPointer() );
			int len = fbb.GetSize();

			std::cout << "create buffer done:" << len << std::endl;

			decode_test( buff,len,schema );
		}
	}
	clock_t interval = clock() - start;
	std::cout << "encode cost " << ((float)interval)/CLOCKS_PER_SEC << std::endl;


	return 0;
}

// ./flatc -b --schema performance.fbs
//g++ -std=c++11 -g3 -o performance performance.cpp -I./include
// 不加优化，100000次为3.09s
// O2优化，100000为0.24s