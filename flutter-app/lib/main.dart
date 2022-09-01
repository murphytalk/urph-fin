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

Future<Pointer<Void>> initUrphFin() {
  final c = Completer<Pointer<Void>>();
  _urphFinInitCompleter = c;
  setupFFI(
      Pointer.fromFunction<Void Function(Pointer<Void>)>(onUrphFinInitDone));
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
      child: Overview(),
    );
  }
}

class AwaitWidget extends StatelessWidget {
  const AwaitWidget({Key? key, required this.caption}) : super(key: key);
  final String caption;
  @override
  Widget build(BuildContext context) {
    return Column(mainAxisAlignment: MainAxisAlignment.center, children: [
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
              child: Directionality(
                  textDirection: TextDirection.ltr, child: Text(caption))))
    ]);
  }
}

class Overview extends StatefulWidget {
  const Overview({Key? key}) : super(key: key);

  @override
  State<Overview> createState() => _OverviewState();
}

class _OverviewState extends State<Overview> {
  Future<int>? _assets;

  _OverviewState() {
    _assets = getAssets();
  }

  List<TableRow> _populateDataTable(BuildContext ctx, int? assetHandler) {
    final rows = <TableRow>[
      const TableRow(children: [
        Text(
          'Asset',
          style: TextStyle(fontWeight: FontWeight.bold),
        ),
        Text(
          'Broker',
          style: TextStyle(fontWeight: FontWeight.bold),
        ),
        Text(
          'Currency',
          style: TextStyle(fontWeight: FontWeight.bold),
        ),
        Text(
          'Market Value',
          textAlign: TextAlign.right,
          style: TextStyle(fontWeight: FontWeight.bold),
        ),
        Text(
          '',
        ),
        Text(
          'Profit',
          textAlign: TextAlign.right,
          style: TextStyle(fontWeight: FontWeight.bold),
        ),
        Text(
          '',
        ),
      ])
    ];

    if (assetHandler == null) return [];

    final ov = getOverview(
            assetHandler, mainCcy, groupByAsset, groupByBroker, groupByCcy)
        .ref;
    for (int i = 0; i < ov.num; i++) {
      final containerContainer = ov.first[i];

      rows.add(TableRow(children: [
        Text(containerContainer.name.toDartString()),
        const Text(''), // lvl2 name
        const Text(''), // lvl3 name
        const Text(''), // Market value
        financeValueText(
            ctx,
            mainCcy,
            containerContainer
                .value_sum_in_main_ccy), // Market value in main CCY
        const Text(''), // Profit
        financeValueText(ctx, mainCcy,
            containerContainer.profit_sum_in_main_ccy), // Profit in main CCY
      ]));

      for (int j = 0; j < containerContainer.num; j++) {
        final container = containerContainer.containers[j];

        rows.add(TableRow(children: [
          //const Text(''), // lvl1 name
          IconButton(
              onPressed: () => {print('expand')},
              icon: const Icon(Icons.expand_less)),
          TableCell(
              verticalAlignment: TableCellVerticalAlignment.middle,
              child: Text(
                container.name.toDartString(),
              )),
          const Text(''), // lvl3 name
          const Text(''), // Market value
          TableCell(
              verticalAlignment: TableCellVerticalAlignment.middle,
              child: financeValueText(ctx, mainCcy,
                  container.value_sum_in_main_ccy)), // Market value in main CCY
          const Text(''), // Profit
          TableCell(
              verticalAlignment: TableCellVerticalAlignment.middle,
              child: financeValueText(ctx, mainCcy,
                  container.profit_sum_in_main_ccy)), // Profit in main CCY
        ]));

        for (int k = 0; k < container.num; k++) {
          final item = container.items[k];
          if (item.value == 0) continue;
          final name = item.name.toDartString();
          final ccy = item.currency.toDartString();
          rows.add(TableRow(children: [
            const Text(''), // lvl1 name
            const Text(''), // lvl2 name
            Text(name),
            TableCell(
                verticalAlignment: TableCellVerticalAlignment.middle,
                child: financeValueText(ctx, ccy, item.value)), // Market value
            TableCell(
                verticalAlignment: TableCellVerticalAlignment.middle,
                child: financeValueText(ctx, mainCcy,
                    item.value_in_main_ccy)), // Market value in main CCY
            TableCell(
                verticalAlignment: TableCellVerticalAlignment.middle,
                child: financeValueText(ctx, ccy, item.profit)), // Profit
            TableCell(
                verticalAlignment: TableCellVerticalAlignment.middle,
                child: financeValueText(ctx, mainCcy,
                    item.profit_in_main_ccy)), // Profit in main CCY
          ]));
        }
      }
    }
    return rows;
  }

  @override
  Widget build(BuildContext context) {
    return FutureBuilder(
        future: _assets,
        builder: (BuildContext ctx, AsyncSnapshot<int> snapshot) {
          if (snapshot.hasData) {
            return SingleChildScrollView(
                child: Padding(
                    padding: const EdgeInsets.all(20),
                    child: Table(
                        columnWidths: const {
                          0: FixedColumnWidth(100),
                          1: FixedColumnWidth(100),
                          2: FixedColumnWidth(80),
                          3: FlexColumnWidth(1),
                          4: FlexColumnWidth(1),
                          5: FlexColumnWidth(1),
                          6: FlexColumnWidth(1),
                        },
                        border: TableBorder.all(width: 1),
                        children: _populateDataTable(ctx, snapshot.data))));
          } else {
            return const Center(child: AwaitWidget(caption: "Loading assets"));
          }
        });
  }
}
