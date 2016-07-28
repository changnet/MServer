#include <ctime>
#include <iostream>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/compiler/importer.h>

int main()
{
	   google::protobuf::compiler::DiskSourceTree disk_source;
        disk_source.MapPath( "","./" );

    google::protobuf::compiler::Importer *importer = new google::protobuf::compiler::Importer( &disk_source,NULL );
    importer->Import( "test.proto" );

    google::protobuf::DynamicMessageFactory *factory = new google::protobuf::DynamicMessageFactory();


    const google::protobuf::Descriptor *descriptor = importer->pool()->FindMessageTypeByName("test");

    const google::protobuf::Message *message = factory->GetPrototype(descriptor);

    clock_t start = clock();
    for ( int count = 0;count < 100000;count ++)
    {
    google::protobuf::Message *msg = message->New();
    if ( !msg )
    {
        std::cerr << "no such message" << std::endl;
        return 1;
    }
	const google::protobuf::Reflection *reflection = msg->GetReflection();

    const google::protobuf::FieldDescriptor *field = NULL;

    field = descriptor->FindFieldByName("el");

    const google::protobuf::Descriptor *sub_descriptor = descriptor->FindNestedTypeByName("element");
    for ( int i = 0;i < 5;i ++ )
    {
    	google::protobuf::Message *element = reflection->AddMessage( msg,field );
        const google::protobuf::Reflection *el_reflection = element->GetReflection();

    	const google::protobuf::FieldDescriptor *el_field = NULL;

    	el_field = sub_descriptor->FindFieldByName("int8_min");
    	el_reflection->SetInt32( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("int8_max");
    	el_reflection->SetInt32( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("uint8_min");
    	el_reflection->SetInt32( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("uint8_max");
    	el_reflection->SetInt32( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("int16_min");
    	el_reflection->SetInt32( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("int16_max");
    	el_reflection->SetInt32( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("uint16_min");
    	el_reflection->SetInt32( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("uint16_max");
    	el_reflection->SetInt32( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("int32_min");
    	el_reflection->SetInt32( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("int32_max");
    	el_reflection->SetInt32( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("uint32_min");
    	el_reflection->SetUInt32( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("uint32_max");
    	el_reflection->SetUInt32( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("int64_min");
    	el_reflection->SetInt64( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("int64_max");
    	el_reflection->SetInt64( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("uint64_min");
    	el_reflection->SetUInt64( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("uint64_max");
    	el_reflection->SetUInt64( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("double_min");
    	el_reflection->SetDouble( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("double_max");
    	el_reflection->SetDouble( element,el_field,0 );

    	el_field = sub_descriptor->FindFieldByName("str");
    	el_reflection->SetString( element,el_field,"abcedf" );
    }

    //std::cout << msg->Utf8DebugString().c_str() << std::endl;

    const int len = 20480;
    char buffer[len] = {0};

    msg->SerializeToArray( buffer,len );
    delete msg;
}

    clock_t t = clock() - start;
    std::cout << "cost time" << "  " << t << "  " << ((float)t)/CLOCKS_PER_SEC << std::endl;

    delete factory;
    delete importer;

	return 0;
}

/*


http://www.cnblogs.com/jacksu-tencent/p/3447310.html
https://www.ibm.com/developerworks/cn/linux/l-cn-gpb/

g++ -g3 -o main main.cpp protobuf_reflection.cpp -I/home/xzc/protobuf-2.6.1/src -L/home/xzc/protobuf-2.6.1/src/.libs -lprotobuf -Wl,-rpath /home/xzc/protobuf-2.6.1/src/.libs
root@debian:/home/xzc/share/github/protobuf-reflection# g++ -g3 -o main main.cpp protobuf_reflection.cpp -I/home/xzc/protobuf-2.6.1/src -L/home/xzc/protobuf-2.6.1/src/.libs -lprotobuf -Wl,-rpath /home/xzc/protobuf-2.6.1/src/.libs
root@debian:/home/xzc/share/github/protobuf-reflection# ./main
cost time  2970000  2.97
root@debian:/home/xzc/share/github/protobuf-reflection# ./main
cost time  3030000  3.03
root@debian:/home/xzc/share/github/protobuf-reflection# ./main
cost time  2950000  2.95
root@debian:/home/xzc/share/github/protobuf-reflection# ./main
cost time  2990000  2.99
root@debian:/home/xzc/share/github/protobuf-reflection# ./main
cost time  2960000  2.96

stream测试
stream packet send cost 12  microsecond
stream packet send cost 11  microsecond
stream packet send cost 11  microsecond
stream packet send cost 11  microsecond
stream packet send cost 11  microsecond
stream packet send cost 11  microsecond
stream packet send cost 9   microsecond
stream packet send cost 9   microsecond
stream packet send cost 8   microsecond
stream packet send cost 8   microsecond
stream packet send cost 11  microsecond
stream packet send cost 9   microsecond
stream packet send cost 8   microsecond
stream packet send cost 8   microsecond

stream packet recv cost 18  microsecond
stream packet recv cost 21  microsecond
stream packet recv cost 18  microsecond
stream packet recv cost 19  microsecond
stream packet recv cost 19  microsecond
stream packet recv cost 21  microsecond
stream packet recv cost 19  microsecond
stream packet recv cost 19  microsecond
stream packet recv cost 18  microsecond
stream packet recv cost 90  microsecond
stream packet recv cost 20  microsecond
stream packet recv cost 18  microsecond

*/