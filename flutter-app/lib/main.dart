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
  setupFFI(
      Pointer.fromFunction<Void Function(Pointer<Void>)>(onUrphFinInitDone));
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
                  appBar: AppBar(
                      // Here we take the value from the MyHomePage object that was created by
                      // the App.build method, and use it to set our appbar title.
                      //title: const Text(TITLE),
                      ),
                  body: Center(
                    // Center is a layout widget. It takes a single child and positions it
                    // in the middle of the parent.
                    child: Column(
                      // Column is also a layout widget. It takes a list of children and
                      // arranges them vertically. By default, it sizes itself to fit its
                      // children horizontally, and tries to be as tall as its parent.
                      //
                      // Invoke "debug painting" (press "p" in the console, choose the
                      // "Toggle Debug Paint" action from the Flutter Inspector in Android
                      // Studio, or the "Toggle Debug Paint" command in Visual Studio Code)
                      // to see the wireframe for each widget.
                      //
                      // Column has various properties to control how it sizes itself and
                      // how it positions its children. Here we use mainAxisAlignment to
                      // center the children vertically; the main axis here is the vertical
                      // axis because Columns are vertical (the cross axis would be
                      // horizontal).
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: const <Widget>[WidthAwareView()],
                    ),
                  ),
                ));
          } else {
            child = const AwaitWidget(caption: "Logging in");
          }
          return Center(child: child);
        });
  }
}

class WidthAwareView extends StatefulWidget {
  const WidthAwareView({Key? key}) : super(key: key);

  @override
  State<WidthAwareView> createState() => _WidthAwareViewState();
}

class _WidthAwareViewState extends State<WidthAwareView> {
  @override
  Widget build(BuildContext context) {
    // this query will trigger view be built whenever screen size is changed
    // https://api.flutter.dev/flutter/widgets/MediaQuery/of.html
    final screenWidth = MediaQuery.of(context).size.width;
    print('build split view, screen width = $screenWidth');
    return const Expanded(
      child: OverviewWidget(),
    );
  }
}
