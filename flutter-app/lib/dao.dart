import 'package:ffi/ffi.dart';
import 'package:urph_fin/dylib_utils.dart';
import 'dart:ffi';
import 'dart:isolate';

void log(String s)
{
  print(s);
}

final dl = dlopenPlatformSpecific('urph-fin-core-dart');
void requestExecuteCallback(dynamic message) {
  final int workAddress = message;
  final work = Pointer<Work>.fromAddress(workAddress);
  log("Dart:   Calling into C to execute callback ($work).");
  executeCallback(work);
  log("Dart:   Done with callback.");
}

final executeCallback = dl.lookupFunction<Void Function(Pointer<Work>),
    void Function(Pointer<Work>)>('execute_callback');

//urph-fin init
final urphFinInitNative = dl.lookupFunction<Bool Function(), bool Function()>('dart_urph_fin_core_init');
final urphFinClose = dl.lookupFunction<Void Function(), void Function()>('urph_fin_core_close');

final regOnInitDoneCallback = dl.lookupFunction<
    Void Function(Int64 sendPort,
        Pointer<NativeFunction<Void Function(Pointer<Void>)>> functionPointer),
    void Function(int sendPort,
        Pointer<NativeFunction<Void Function(Pointer<Void>)>> functionPointer)>('register_init_callback');

class Work extends Opaque {}

void setupFFI(Pointer<NativeFunction<Void Function(Pointer<Void>)>> initDoneCallbackFP)
{
  final initializeApi = dl.lookupFunction<IntPtr Function(Pointer<Void>),
      int Function(Pointer<Void>)>("InitializeDartApiDL");
  final r = initializeApi(NativeApi.initializeApiDLData);
  log("initApi $r");

  final interactiveCppRequests = ReceivePort()..listen(requestExecuteCallback);
  final int nativePort = interactiveCppRequests.sendPort.nativePort;
  regOnInitDoneCallback(nativePort, initDoneCallbackFP);
  final initResult = urphFinInitNative();
  log('urph-fin init result: $initResult');
}

class Strings extends Struct{
  @Int32()
  external int capacity;
  external Pointer<Pointer<Utf8>> strs;
  external Pointer<Pointer<Utf8>> last_str;
}
typedef GetAllBrokersNameFunc = Pointer<Strings> Function();
final urphFinGetAllBrokersName = dl.lookupFunction<GetAllBrokersNameFunc, GetAllBrokersNameFunc>('get_all_broker_names');
final urphFinFreeStrings = dl.lookupFunction<Void Function(Pointer<Strings>), void Function(Pointer<Strings>)>('free_strings');

/*
void dumpAllBrokerNames() {
  final brokerNames = urphFinGetAllBrokersName();
  log("got ${brokerNames.ref.capacity}");
  final strs = brokerNames.ref.strs;
  for (int i = 0; i < brokerNames.ref.capacity; i++) {
    log("Broker ${strs[i].toDartString()}");
  }
  urphFinFreeStrings(brokerNames);
}
*/