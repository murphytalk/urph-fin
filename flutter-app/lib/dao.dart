import 'dart:async';

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

// overview
class OverviewItem extends Struct{
  external Pointer<Pointer<Utf8>> name;
  @Double()
  external double value;
  @Double()
  external double value_in_main_ccy;
  @Double()
  external double profit;
  @Double()
  external double profit_in_main_ccy;
}

class OverviewItemContainer extends Struct{
  external Pointer<Pointer<Utf8>> name;
  external Pointer<Pointer<Utf8>> item_name;
  @Double()
  external double value_sum_in_main_ccy;
  @Double()
  external double profit_sum_in_main_ccy;
  @Int32()
  external int num;
  external Pointer<OverviewItem> items;
}

class OverviewItemContainerContainer extends Struct{
  external Pointer<Pointer<Utf8>> name;
  external Pointer<Pointer<Utf8>> item_name;
  @Double()
  external double value_sum_in_main_ccy;
  @Double()
  external double profit_sum_in_main_ccy;
  @Int32()
  external int num;
  external Pointer<OverviewItemContainer> containers;
}

class Overview extends Struct{
  external Pointer<Pointer<Utf8>> item_name;
  @Double()
  external double value_sum_in_main_ccy;
  @Double()
  external double profit_sum_in_main_ccy;
  @Int32()
  external int num;
  external Pointer<OverviewItemContainerContainer> first;
}
final urphFinLoadAssets = dl.lookupFunction<Int32 Function(), int Function()>('load_assets');
final urphFinFreeAssets = dl.lookupFunction<Void Function(Int32), void Function(int)>('free_assets');

const int groupByAsset  = 0;
const int groupByBroker = 1;
const int groupByCcy    = 2;
final urphFinGetOverview = dl.lookupFunction<
    Pointer<Overview> Function(Int32, Pointer<Utf8>,Uint8,Uint8,Uint8),
    Pointer<Overview> Function(int  , Pointer<Utf8>,int,int,int)>('get_overview');

Future<int>? _assetsFuture;
Future<int> getAssets()
{
  final completer = Completer<int>();
  _assetsFuture = completer.future;
  return completer.future;
}

Pointer<Overview> getOverview(int assetHandler, String mainCcy, int lvl1, int lvl2, int lvl3)
{
  final nativeUtf8Ptr = mainCcy.toNativeUtf8();
  final p = urphFinGetOverview(assetHandler, nativeUtf8Ptr , lvl1, lvl2, lvl3);
  malloc.free(nativeUtf8Ptr);
  return p;
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