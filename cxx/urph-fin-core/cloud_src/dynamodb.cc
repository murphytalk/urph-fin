#include <aws/core/Aws.h>
#include "../src/storage/storage.hxx"

class AwsDao{

};

IDataStorage * create_cloud_instance(OnDone onInitDone) { return new Storage<AwsDao>(onInitDone); }