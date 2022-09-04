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

class MyApp extends StatelessWidget {
  const MyApp({Key? key}) : super(key: key);

  // root widget
  @override
  Widget build(BuildContext context) {
    return FutureBuilder(
        future: initUrphFin(),
        builder: (c, snapshot) {
          Widget child;
          if (snapshot.hasData) {
            child = MaterialApp(
              theme: ThemeData(
                // This is the theme of your application.
                //
                // Try running your application with "flutter run". You'll see the
                // application has a blue toolbar. Then, without quitting the app, try
                // changing the primarySwatch below to Colors.green and then invoke
                // "hot reload" (press "r" in the console where you ran "flutter run",
                // or simply save your changes to "hot reload" in a Flutter IDE).
                // Notice that the counter didn't reset back to zero; the application
                // is not restarted.
                primarySwatch: Colors.blue,
              ),
              home: Scaffold(
                appBar: AppBar(),
                body: const Center(child: WidthAwareView()),
                drawer: Drawer(
                  child: ListView(
                    padding: EdgeInsets.zero,
                    children: const <Widget>[
                      /*
                      DrawerHeader(
                        decoration: BoxDecoration(
                          color: Colors.blue,
                        ),
                        child: Text(
                          'urph-fin',
                          style: TextStyle(
                            color: Colors.white,
                            fontSize: 18,
                          ),
                        ),
                      ),
                      */
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
            child = const AwaitWidget(caption: "Logging in");
          }
          return Center(child: child);
        });
  }
}

class WidthAwareView extends StatelessWidget {
  const WidthAwareView({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    // this query will trigger view be built whenever screen size is changed
    // https://api.flutter.dev/flutter/widgets/MediaQuery/of.html
    final screenWidth = MediaQuery.of(context).size.width;
    print('build split view, screen width = $screenWidth');
    return const OverviewWidget();
  }
}
