import 'package:urph_fin/dylib_utils.dart';
import 'dart:ffi';
import 'dart:isolate';

final dl = dlopenPlatformSpecific('urph-fin-core-dart');
void requestExecuteCallback(dynamic message) {
  final int work_address = message;
  final work = Pointer<Work>.fromAddress(work_address);
  print("Dart:   Calling into C to execute callback ($work).");
  executeCallback(work);
  print("Dart:   Done with callback.");
}

final executeCallback = dl.lookupFunction<Void Function(Pointer<Work>),
    void Function(Pointer<Work>)>('execute_callback');

//urph-fin init
final urphFinInitNative = dl.lookupFunction<Bool Function(), bool Function()>('dart_urph_fin_core_init');

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
  print("initApi $r");

  final interactiveCppRequests = ReceivePort()..listen(requestExecuteCallback);
  final int nativePort = interactiveCppRequests.sendPort.nativePort;
  regOnInitDoneCallback(nativePort, initDoneCallbackFP);
  final initResult = urphFinInitNative();
  print('urph-fin init result: $initResult');
}