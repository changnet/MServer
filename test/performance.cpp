#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"

#include <cstring>
#include <iostream>

/* fbs文件规则 https://google.github.io/flatbuffers/md__schemas.html
 * Any table can be a root in a buffer. The root type is mostly for use by JSON,
 * which doesn't specify any types, so we need to know where to start
 * 编码原理:https://google.github.io/flatbuffers/md__internals.html
 *
 * 内存分布:http://www.race604.com/flatbuffers-intro/
 */

flatbuffers::uoffset_t encode_object( const reflection::Object *obj )
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
	const auto *fields = obj.fields();

	flatbuffers::FlatBufferBuilder fbb;
	flatbuffers::uoffset_t start = fbb.StartTable();

	flatbuffers::Vector<flatbuffers::Offset<Field>>::iterator itr = fields->begin();
    while ( itr != fields->end() )
    {
    	// 从lua转时，直接将name当key push取值，如果取不到而field.is_require则报错
        auto &field = *itr;
        uint16_t off = field.offset();
        switch ( field.type()->base_type() )
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

    return fbb.EndTable(start, pObj.fields()->size());
}

flatbuffers::uoffset_t encode_vector( const reflection::Object *obj )
{
	std::vector< flatbuffers::Offset<void> > vOffsets;
    for (int i = 0;i < 5;i ++)
    {
    	vOffsets.push_back( encode_object( obj ) );
    }

    flatbuffers::FlatBufferBuilder fbb;
    return fbb.CreateVector(vOffsets).o;
}

flatbuffers::uoffset_t encode_test( const reflection::Schema *schema )
{
	const reflection::Object* pObj ＝ schema.objects().LookupByKey("test");
	if ( !pObj )
	{
	  std::cerr << "no object found" << std::endl;
	  return 2;
	}

	const auto *test_fields = pObj.fields();
	auto &element_field = *test_fields->LookupByKey("el");

	reflection::Object *obj = schema.objects()->Get(element_field.type()->index());

	flatbuffers::FlatBufferBuilder fbb;
	flatbuffers::uoffset_t start = fbb.StartTable();
	// for ( field)
	fbb.AddOffset(element_field.offset(),flatbuffers::Offset<void>( encode_vector(obj) ));

	return fbb.EndTable(start, pObj.fields()->size());
}

int main()
{
	std::string bfbsfile;
	if ( !flatbuffers::LoadFile("performance.bfbs", true, &bfbsfile) )
	{
	   std::cerr << "load bfbs fail" << std::endl;
		return 1;
	}

	auto &schema = *reflection::GetSchema(bfbsfile.c_str());
	
	flatbuffers::uoffset_t offset = encode_test( schema );
	fbb.Finish(flatbuffers::Offset<void>(offset));

	const char *buff = fbb.GetBufferPointer();
	int len = fbb.GetSize();

	return 0;
}