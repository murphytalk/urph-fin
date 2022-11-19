import 'dart:async';

import 'package:ffi/ffi.dart';
import 'package:urph_fin/utils.dart';
import 'dart:ffi';
import 'dart:isolate';

void log(String s) {
  print(s);
}

DynamicLibrary _dlopenPlatformSpecific(String name, {String path = ""}) {
  String fullPath = platformPath(name, path);
  return DynamicLibrary.open(fullPath);
}

final dl = _dlopenPlatformSpecific('urph-fin-core-dart');
void requestExecuteCallback(dynamic message) {
  final int workAddress = message;
  final work = Pointer<Work>.fromAddress(workAddress);
  log("Dart:   Calling into C to execute callback ($work).");
  executeCallback(work);
  log("Dart:   Done with callback.");
}

final executeCallback =
    dl.lookupFunction<Void Function(Pointer<Work>), void Function(Pointer<Work>)>('execute_callback');

//urph-fin init
final urphFinInitNative = dl.lookupFunction<Bool Function(), bool Function()>('dart_urph_fin_core_init');
final urphFinClose = dl.lookupFunction<Void Function(), void Function()>('urph_fin_core_close');

final regOnInitDoneCallback = dl.lookupFunction<
    Void Function(Int64 sendPort, Pointer<NativeFunction<Void Function(Pointer<Void>)>>),
    void Function(int sendPort, Pointer<NativeFunction<Void Function(Pointer<Void>)>>)>('register_init_callback');

class Work extends Opaque {}

void setupFFI(Pointer<NativeFunction<Void Function(Pointer<Void>)>> initDoneCallbackFP) {
  final initializeApi =
      dl.lookupFunction<IntPtr Function(Pointer<Void>), int Function(Pointer<Void>)>("InitializeDartApiDL");
  final r = initializeApi(NativeApi.initializeApiDLData);
  log("initApi $r");

  final interactiveCppRequests = ReceivePort()..listen(requestExecuteCallback);
  final int nativePort = interactiveCppRequests.sendPort.nativePort;

  regOnInitDoneCallback(nativePort, initDoneCallbackFP);
  regOnAssetsLoadedCallback(Pointer.fromFunction<Void Function(Pointer<Void>, Int32)>(_onAssetLoadedCB));

  final initResult = urphFinInitNative();
  log('urph-fin init result: $initResult');
}

class Strings extends Struct {
  @Int32()
  external int capacity;
  external Pointer<Pointer<Utf8>> strs;
  external Pointer<Pointer<Utf8>> last_str;
}

class Quote extends Struct {
  external Pointer<Utf8> symbol;
  @Int64()
  external int date;
  @Double()
  external double rate;
}

class Quotes extends Struct {
  @Int32()
  external int num;
  external Pointer<Quote> first;
}

// asset overview
class OverviewItem extends Struct {
  external Pointer<Utf8> name;
  external Pointer<Utf8> currency;
  @Double()
  external double value;
  @Double()
  external double value_in_main_ccy;
  @Double()
  external double profit;
  @Double()
  external double profit_in_main_ccy;
}

class OverviewItemContainer extends Struct {
  external Pointer<Utf8> name;
  external Pointer<Utf8> item_name;
  @Double()
  external double value_sum_in_main_ccy;
  @Double()
  external double profit_sum_in_main_ccy;
  @Int32()
  external int num;
  external Pointer<OverviewItem> items;
}

class OverviewItemList extends Struct {
  @Int32()
  external int num;
  external Pointer<OverviewItem> first;
}

class OverviewItemContainerContainer extends Struct {
  external Pointer<Utf8> name;
  external Pointer<Utf8> item_name;
  @Double()
  external double value_sum_in_main_ccy;
  @Double()
  external double profit_sum_in_main_ccy;
  @Int32()
  external int num;
  external Pointer<OverviewItemContainer> containers;
}

class Overview extends Struct {
  external Pointer<Utf8> item_name;
  @Double()
  external double value_sum_in_main_ccy;
  @Double()
  external double profit_sum_in_main_ccy;
  @Int32()
  external int num;
  external Pointer<OverviewItemContainerContainer> first;
}

final regOnAssetsLoadedCallback = dl.lookupFunction<
    Void Function(Pointer<NativeFunction<Void Function(Pointer<Void>, Int32)>>),
    void Function(Pointer<NativeFunction<Void Function(Pointer<Void>, Int32)>>)>('register_asset_loaded_callback');

final _urphFinLoadAssets = dl.lookupFunction<Void Function(), void Function()>('dart_urph_fin_load_assets');
final urphFinFreeAssets = dl.lookupFunction<Void Function(Int32), void Function(int)>('free_assets');

final _urphFinGetOverview = dl.lookupFunction<Pointer<Overview> Function(Int32, Pointer<Utf8>, Uint8, Uint8, Uint8),
    Pointer<Overview> Function(int, Pointer<Utf8>, int, int, int)>('get_overview');

final urphFinFreeOverview =
    dl.lookupFunction<Void Function(Pointer<Overview>), void Function(Pointer<Overview>)>('free_overview');

Completer<int>? _assetsHandleCompleter;
void _onAssetLoadedCB(Pointer<Void> p, int handle) {
  _assetsHandleCompleter?.complete(handle);
}

Future<int> getAssets() {
  final completer = Completer<int>();
  _assetsHandleCompleter = completer;
  _urphFinLoadAssets();
  return completer.future;
}

Pointer<Overview> getOverview(
    int assetHandler, String mainCcy, OverviewGroup lvl1, OverviewGroup lvl2, OverviewGroup lvl3) {
  final nativeUtf8Ptr = mainCcy.toNativeUtf8();
  final p = _urphFinGetOverview(assetHandler, nativeUtf8Ptr, lvl1, lvl2, lvl3);
  malloc.free(nativeUtf8Ptr);
  return p;
}

final _urphFinGetSumGroupByAsset = dl.lookupFunction<Pointer<OverviewItemList> Function(Int32, Pointer<Utf8>),
    Pointer<OverviewItemList> Function(int, Pointer<Utf8>)>('get_sum_group_by_asset');

final _urphFinGetSumGroupByBroker = dl.lookupFunction<Pointer<OverviewItemList> Function(Int32, Pointer<Utf8>),
    Pointer<OverviewItemList> Function(int, Pointer<Utf8>)>('get_sum_group_by_broker');

Pointer<OverviewItemList> _getSumGroup(
    String mainCcy, int assetHandler, Pointer<OverviewItemList> Function(int, Pointer<Utf8>) func) {
  final nativeUtf8Ptr = mainCcy.toNativeUtf8();
  final p = func(assetHandler, nativeUtf8Ptr);
  malloc.free(nativeUtf8Ptr);
  return p;
}

Pointer<OverviewItemList> getSumGroupByAsset(int assetHandle, String mainCcy) {
  return _getSumGroup(mainCcy, assetHandle, _urphFinGetSumGroupByAsset);
}

Pointer<OverviewItemList> getSumGroupByBroker(int assetHandle, String mainCcy) {
  return _getSumGroup(mainCcy, assetHandle, _urphFinGetSumGroupByBroker);
}

final urphFinFreeOverviewItemList =
    dl.lookupFunction<Void Function(Pointer<OverviewItemList>), void Function(Pointer<OverviewItemList>)>(
        'free_overview_item_list');

final urphFinFreeStrings =
    dl.lookupFunction<Void Function(Pointer<Strings>), void Function(Pointer<Strings>)>('free_strings');

final urphFinGetAllCcy =
    dl.lookupFunction<Pointer<Strings> Function(Int32), Pointer<Strings> Function(int)>('get_all_ccy');

final urphFinGetAllCcyPairsQuote =
    dl.lookupFunction<Pointer<Quotes> Function(Int32), Pointer<Quotes> Function(int)>('get_all_ccy_pairs_quote');

final urphFinGetLatestQuotes = dl.lookupFunction<Pointer<Quotes> Function(Int64, Int32, Pointer<Utf8>),
    Pointer<Quotes> Function(int, int, Pointer<Utf8>)>('get_latest_quotes_delimeter');

final urphFinFreeQuotes =
    dl.lookupFunction<Void Function(Pointer<Quotes>), void Function(Pointer<Quotes>)>('free_quotes');

typedef GetAllBrokersNameFunc = Pointer<Strings> Function();
final urphFinGetAllBrokersName =
    dl.lookupFunction<GetAllBrokersNameFunc, GetAllBrokersNameFunc>('get_all_broker_names');

void dumpAllBrokerNames() {
  final brokerNames = urphFinGetAllBrokersName();
  print("got ${brokerNames.ref.capacity}");
  final strs = brokerNames.ref.strs;
  for (int i = 0; i < brokerNames.ref.capacity; i++) {
    print("Broker ${strs[i].toDartString()}");
  }
  urphFinFreeStrings(brokerNames);
}
