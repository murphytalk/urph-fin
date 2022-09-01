import 'dart:ffi';

import 'package:ffi/ffi.dart';
import 'package:flutter/material.dart';
import 'package:urph_fin/shared_widgets.dart';
import 'package:urph_fin/utils.dart';
import 'package:urph_fin/dao.dart';

class Overview extends StatefulWidget {
  const Overview({Key? key}) : super(key: key);

  @override
  State<Overview> createState() => _OverviewState();
}

const mainCcy = 'JPY';
class _OverviewState extends State<Overview> {
  Future<int>? _assets;
  OverviewGroup _lvl1 = groupByAsset;
  OverviewGroup _lvl2 = groupByBroker;
  OverviewGroup _lvl3 = groupByCcy;

  _OverviewState() {
    _assets = getAssets();
  }

  List<TableRow> _populateDataTable(BuildContext ctx, TextStyle headerTxtStyle, int? assetHandler) {
    if (assetHandler == null) return [];
    final ov = getOverview(assetHandler, mainCcy, _lvl1, _lvl2, _lvl3).ref;

    final rows = <TableRow>[
      TableRow(children: [
        Text(
          'Asset',
          style: headerTxtStyle,
        ),
        Text(
          'Broker',
          style: headerTxtStyle,
        ),
        Text(
          'Currency',
          style: headerTxtStyle,
        ),
        Text(
          'Market Value',
          style: headerTxtStyle,
          textAlign: TextAlign.right,
        ),
        financeValueText(ctx, mainCcy, ov.value_sum_in_main_ccy, positiveValueColor: Colors.blue),
        Text(
          'Profit',
          style: headerTxtStyle,
          textAlign: TextAlign.right,
        ),
        financeValueText(ctx, mainCcy, ov.profit_sum_in_main_ccy, positiveValueColor: Colors.blue),
      ])
    ];


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
          const Text(''), // lvl1 name
          TableCell(
              verticalAlignment: TableCellVerticalAlignment.middle,
              child: Row(
                  children:[
                    IconButton(
                        onPressed: () => {print('expand')},
                        icon: const Icon(Icons.expand_less)),
                    Text(container.name.toDartString())
                  ])
          ),
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
            const headerTxtStyle = TextStyle(fontWeight: FontWeight.bold);
            const headerTxtPadding = 10;
            return SingleChildScrollView(
                child: Padding(
                    padding: const EdgeInsets.all(20),
                    child: Table(
                        columnWidths: {
                          0: FixedColumnWidth(getGroupTextSize(ctx, headerTxtStyle, _lvl1).width + headerTxtPadding),
                          1: FixedColumnWidth(getGroupTextSize(ctx, headerTxtStyle, _lvl2).width + headerTxtPadding),
                          2: FixedColumnWidth(getGroupTextSize(ctx, headerTxtStyle, _lvl3).width + headerTxtPadding),
                          3: const FlexColumnWidth(1),
                          4: const FlexColumnWidth(1),
                          5: const FlexColumnWidth(1),
                          6: const FlexColumnWidth(1),
                        },
                        //border: const TableBorder(bottom: BorderSide(color: Colors.black)),
                        children: _populateDataTable(ctx, headerTxtStyle, snapshot.data))));
          } else {
            return const Center(child: AwaitWidget(caption: "Loading assets"));
          }
        });
  }
}
