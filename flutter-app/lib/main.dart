import 'dart:async';
import 'dart:ffi';

import 'package:flutter/material.dart';
import 'package:window_manager/window_manager.dart';
import 'package:urph_fin/dao.dart' hide Overview;
import 'package:urph_fin/shared_widgets.dart';
import 'package:urph_fin/overview.dart';

Completer<Pointer<Void>>? _urphFinInitCompleter;
void onUrphFinInitDone(Pointer<Void> p) {
  log("urph-fin init done, starting flutter app");
  _urphFinInitCompleter?.complete(p);
}

Future<Pointer<Void>> initUrphFin() {
  final c = Completer<Pointer<Void>>();
  _urphFinInitCompleter = c;
  setupFFI(Pointer.fromFunction<Void Function(Pointer<Void>)>(onUrphFinInitDone));
  return c.future;
}

const theTitle = 'UrphFin';

void main() {
  runApp(const MyApp());
  windowManager.waitUntilReadyToShow().then((_) async {
    //await windowManager.setAsFrameless();
    await windowManager.setTitle(theTitle);
  });
}

enum _Status {
  login,
  loadingAssets,
  loadedAssets,
}

class _AssetsManager {
  final Future<Pointer<Void>> _initCloudFuture;
  late Completer<int> _assetHandle;
  late Future<int> _assetsFuture;

  _Status _status = _Status.login;
  String get status {
    switch (_status) {
      case _Status.login:
        return "Logging in";
      case _Status.loadingAssets:
        return "Loading Assets";
      case _Status.loadedAssets:
        return "Loaded Assets";
    }
  }

  late int _assets;
  int get assets {
    return _status != _Status.loadedAssets ? 0 : _assets;
  }

  _AssetsManager() : _initCloudFuture = initUrphFin();

  void load(void Function(VoidCallback) setState) {
    _assetHandle = Completer<int>();
    _initCloudFuture.then((_) {
      setState(() => _status = _Status.loadingAssets);
      _assetsFuture = getAssets();
      _assetsFuture.then((handle) {
        _status = _Status.loadedAssets;
        _assets = handle;
        _assetHandle.complete(handle);
      });
    }); //todo: onError
  }

  Future<int> getAssetsFuture() {
    return _assetHandle.future;
  }
}

class MyApp extends StatefulWidget {
  const MyApp({Key? key}) : super(key: key);

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  final _AssetsManager _assetsManager = _AssetsManager();

  List<Widget> _buildFxRates() {
    return [];
  }

  @override
  void initState() {
    super.initState();
    _assetsManager.load((cb) => setState(cb));
  }

  // root widget
  @override
  Widget build(BuildContext context) {
    return FutureBuilder(
        future: _assetsManager.getAssetsFuture(),
        builder: (c, AsyncSnapshot<int> snapshot) {
          Widget child;
          if (snapshot.hasData) {
            child = MaterialApp(
              debugShowCheckedModeBanner: false,
              theme: ThemeData(
                primarySwatch: Colors.blue,
              ),
              home: Scaffold(
                appBar: AppBar(actions: _buildFxRates()),
                body: Center(child: OverviewWidget(_assetsManager.assets)),
                drawer: Drawer(
                  child: ListView(
                    padding: EdgeInsets.zero,
                    children: const <Widget>[
                      ListTile(
                        leading: Icon(Icons.message),
                        title: Text('Messages'),
                        mouseCursor: SystemMouseCursors.click,
                      ),
                      ListTile(
                        leading: Icon(Icons.account_circle),
                        title: Text('Profile'),
                        mouseCursor: SystemMouseCursors.click,
                      ),
                      ListTile(
                        leading: Icon(Icons.settings),
                        title: Text('Settings'),
                        mouseCursor: SystemMouseCursors.click,
                      ),
                    ],
                  ),
                ),
              ),
            );
          } else {
            child = AwaitWidget(caption: _assetsManager.status);
          }
          return Center(child: child);
        });
  }
}
