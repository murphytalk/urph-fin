import 'dart:async';

import 'package:ffi/ffi.dart';
import 'package:flutter/material.dart';
import 'dart:ffi';
import 'package:urph_fin/dao.dart';
import 'package:urph_fin/utils.dart';
import 'package:window_manager/window_manager.dart';

Completer<Pointer<Void>>? _urphFinInitCompleter;
void onUrphFinInitDone(Pointer<Void> p) {
  log("urph-fin init done, starting flutter app");
  _urphFinInitCompleter?.complete(p);
}

Future<Pointer<Void>> initUrphFin()
{
  final c = Completer<Pointer<Void>>();
  _urphFinInitCompleter = c;
  setupFFI(Pointer.fromFunction<Void Function(Pointer<Void>)>(onUrphFinInitDone));
  return c.future;
}

const theTitle = 'UrphFin';
const mainCcy = 'JPY';

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
        builder: (c, snapshot){
          Widget child;
          if(snapshot.hasData) {
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
          }
          else{
            child = const AwaitWidget(caption: "Logging in");
          }
          return Center(
              child: child
          );
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
      child: Overview(),
    );
  }
}

class AwaitWidget extends StatelessWidget {
  const AwaitWidget({Key? key, required this.caption}) : super(key: key);
  final String caption;
  @override
  Widget build(BuildContext context) {
    return
      Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const SizedBox(
              width: 60,
              height: 60,
              child: CircularProgressIndicator(),
            ),
            DefaultTextStyle(
              style: Theme.of(context).textTheme.headline2!,
              textAlign: TextAlign.center,
              child: Padding(
              padding: const EdgeInsets.only(top: 16),
              child: Directionality(textDirection: TextDirection.ltr, child: Text(caption))
            )
            )
          ]
      );
  }
}

Widget _value(BuildContext ctx, String ccy, double value)
{
  return Text(formatCcy(ccy, value),
      textAlign: TextAlign.right,
      style: TextStyle( color: value < 0 ? Colors.red : Colors.black));
}

class Overview extends StatefulWidget {
  const Overview({Key? key}) : super(key: key);

  @override
  State<Overview> createState() => _OverviewState();
}

class _OverviewState extends State<Overview> {
  List<DataRow> _populateDataTable(BuildContext ctx, int? assetHandler){
    if(assetHandler == null) return [];
    final rows = <DataRow>[];
    final ov = getOverview(assetHandler, mainCcy, groupByAsset, groupByBroker, groupByCcy).ref;
    for(int i = 0; i < ov.num; i++){
      final containerContainer = ov.first[i];

      rows.add(DataRow(
          cells: [
            DataCell(Text(containerContainer.name.toDartString())),
            const DataCell(Text('')), // lvl2 name
            const DataCell(Text('')), // lvl3 name
            const DataCell(Text('')), // Market value
            DataCell(_value(ctx,mainCcy, containerContainer.value_sum_in_main_ccy)), // Market value in main CCY
            const DataCell(Text('')), // Profit
            DataCell(_value(ctx,mainCcy, containerContainer.profit_sum_in_main_ccy)), // Profit in main CCY
          ]
      ));

      for(int j = 0 ; j < containerContainer.num; j++){
        final container = containerContainer.containers[j];

        rows.add(DataRow(
            cells: [
              const DataCell(Text('')), // lvl1 name
              DataCell(Text(container.name.toDartString())),
              const DataCell(Text('')), // lvl3 name
              const DataCell(Text('')), // Market value
              DataCell(_value(ctx,mainCcy,container.value_sum_in_main_ccy)), // Market value in main CCY
              const DataCell(Text('')), // Profit
              DataCell(_value(ctx,mainCcy,container.profit_sum_in_main_ccy)), // Profit in main CCY
            ]
        ));

        for(int k = 0; k < container.num ; k++){
          final item = container.items[k];
          final name = item.name.toDartString();
          rows.add(DataRow(
            cells: [
              const DataCell(Text('')), // lvl1 name
              const DataCell(Text('')), // lvl2 name
              DataCell(Text(name)),
              //TODO: overview_item needs ccy
              DataCell(_value(ctx,name,item.value)),              // Market value
              DataCell(_value(ctx,name,item.value_in_main_ccy)),  // Market value in main CCY
              DataCell(_value(ctx,name,item.profit)),             // Profit
              DataCell(_value(ctx,name,item.profit_in_main_ccy)), // Profit in main CCY
            ]
          ));
        }
      }
    }
    return rows;
  }
  @override
  Widget build(BuildContext context) {
    return FutureBuilder(
      future: getAssets(),
      builder: (BuildContext ctx, AsyncSnapshot<int> snapshot){
        if(snapshot.hasData) {
          return SingleChildScrollView(
            child:
            DataTable(columns: const <DataColumn>[
            DataColumn(
              label: Text(
                'Asset',
                style:  TextStyle(fontWeight: FontWeight.bold),
              ),
            ),
            DataColumn(
              label: Text(
                'Broker',
                style: TextStyle(fontWeight: FontWeight.bold),
              ),
            ),
            DataColumn(
              label: Text(
                'Currency',
                style: TextStyle(fontWeight: FontWeight.bold),
              ),
            ),
            DataColumn(
              label: Text(
                'Market Value',
                style: TextStyle(fontWeight: FontWeight.bold),
              ),
            ),
            DataColumn(
              label: Text(
                'Market Value ($mainCcy)',
                style: TextStyle(fontWeight: FontWeight.bold),
              ),
            ),
            DataColumn(
              label: Text(
                'Profit',
                style: TextStyle(fontWeight: FontWeight.bold),
              ),
            ),
            DataColumn(
              label: Text(
                'Profit ($mainCcy)',
                style: TextStyle(fontWeight: FontWeight.bold),
              ),
            ),
          ], rows: _populateDataTable(ctx, snapshot.data)));
        }
        else{
          return const Center(child: AwaitWidget(caption: "Loading assets"));
        }
      }
    );

  }
}
