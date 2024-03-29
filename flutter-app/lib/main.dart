import 'dart:async';
import 'dart:ffi';
import 'dart:ui' as ui;

import 'package:ffi/ffi.dart';
import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:urph_fin/utils.dart';
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

const _theTitle = 'UrphFin';
const _saveWinPosTimerIntervalSeconds = 10;
const _configWinPos = 'WinPos';

class _WinListener extends WindowListener{
  Rect _lastPos = const Rect.fromLTWH(0,0,0,0);
  Rect _savedPos = const Rect.fromLTWH(0,0,0,0);

  _WinListener(){
    Timer.periodic(const Duration(seconds: _saveWinPosTimerIntervalSeconds), (_) {_savePos();});
  }

  void _rememberPos(){
    windowManager.getBounds().then((rect) => {_lastPos = rect });
  }

  void _savePos(){
    if(_savedPos != _lastPos) {
      final winPos = '${_lastPos.left},${_lastPos.top},${_lastPos.width},${_lastPos.height}';
      //print('window resized: ${winPos}');
      SharedPreferences.getInstance().then((pref) => pref.setString(_configWinPos, winPos));
      _savedPos = _lastPos;
    }
  }

  void restoreWinPos(){
    SharedPreferences.getInstance().then((pref) {
        final pos = pref.getString(_configWinPos);
        if(pos != null){
          final values = pos.split(',');
          final l = double.parse(values[0]);
          final t = double.parse(values[1]);
          final w = double.parse(values[2]);
          final h = double.parse(values[3]);
          windowManager.setBounds(Rect.fromLTWH(l, t, w, h));
        }
    });
  }

  @override
  void onWindowMove() { _rememberPos(); }
  @override
  void onWindowResize() { _rememberPos();}
}

void main() {
  runApp(const MyApp());
  windowManager.waitUntilReadyToShow().then((_) async {
    //await windowManager.setAsFrameless();
    await windowManager.setTitle(_theTitle);
  });
  final cfg = _WinListener();
  windowManager.addListener(cfg);
  cfg.restoreWinPos();
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

  double? progress;

  late int _assets;
  int get assets {
    return _status != _Status.loadedAssets ? 0 : _assets;
  }

  _AssetsManager() : _initCloudFuture = initUrphFin();

  void load(void Function(VoidCallback) setState) {
    _assetHandle = Completer<int>();
    _initCloudFuture.then((_) {
      setState(() => _status = _Status.loadingAssets);
      _assetsFuture = getAssets((prg) {
        setState(() => progress = prg);
      });
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
    List<Widget> items = [];
    final all = urphFinGetAllCcyPairsQuote(_assetsManager.assets);
    final allRef = all.ref;

    for (int i = 0; i < allRef.num; ++i) {
      final q = allRef.first[i];
      items.add(Row(
        children: [
          Text(q.symbol.toDartString()),
          Container(
            margin: const EdgeInsets.only(left: 5.0, right: 10.0),
            child: Text(formatNum(q.rate)),
          )
        ],
      ));
    }
    items.add(Row(children: [
      Container(margin: const EdgeInsets.only(right: 10.0), child: Text(formatTimestamp(allRef.first[0].date)))
    ]));
    urphFinFreeQuotes(all);
    return items;
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
            child = Directionality(
              textDirection: ui.TextDirection.ltr,
              child: AwaitWidget(
                caption: _assetsManager.status,
                progress: _assetsManager.progress,
              ),
            );
          }
          return child;
        });
  }
}
