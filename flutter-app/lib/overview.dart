import 'dart:ffi';

import 'package:ffi/ffi.dart';
import 'package:flutter/material.dart';
import 'package:urph_fin/shared_widgets.dart';
import 'package:urph_fin/utils.dart';
import 'package:urph_fin/dao.dart';

class OverviewWidget extends StatefulWidget {
  const OverviewWidget({Key? key}) : super(key: key);

  @override
  State<OverviewWidget> createState() => _OverviewWidgetState();
}

const mainCcy = 'JPY';

enum Level {
  lvl1,
  lvl2,
  leaf;
}

class TableItem {
  final Level level;
  final String name;
  final String ccy;
  final double valueInMainCcy;
  final double profitInMainCcy;
  final double value;
  final double profit;
  bool expanded = false;

  TableItem(this.level, this.name, this.valueInMainCcy, this.profitInMainCcy)
      : value = 0,
        profit = 0,
        ccy = '';

  TableItem.leaf(this.name, this.ccy, this.value, this.valueInMainCcy, this.profit, this.profitInMainCcy)
      : level = Level.leaf;
}

class TableItems {
  final List<TableItem> _items = [];
  final TextStyle _headerTxtStyle;
  late double _valueInMainCcy;
  late double _profitInMainCcy;

  TableItems(this._headerTxtStyle, Pointer<Overview> overview) {
    final ov = overview.ref;
    _valueInMainCcy = ov.value_sum_in_main_ccy;
    _profitInMainCcy = ov.profit_sum_in_main_ccy;

    for (int i = 0; i < ov.num; i++) {
      final containerContainer = ov.first[i];
      _items.add(TableItem(Level.lvl1, containerContainer.name.toDartString(), containerContainer.value_sum_in_main_ccy,
          containerContainer.profit_sum_in_main_ccy));

      for (int j = 0; j < containerContainer.num; j++) {
        final container = containerContainer.containers[j];
        _items.add(TableItem(Level.lvl2, container.name.toDartString(), container.value_sum_in_main_ccy,
            container.profit_sum_in_main_ccy));

        for (int k = 0; k < container.num; k++) {
          final item = container.items[k];
          if (item.value == 0) continue;
          _items.add(TableItem.leaf(item.name.toDartString(), item.currency.toDartString(), item.value,
              item.value_in_main_ccy, item.profit, item.profit_in_main_ccy));
        }
      }
    }
  }

  // for unit tests
  TableItems.withItems(List<TableItem> i, this._valueInMainCcy, this._profitInMainCcy)
      : _headerTxtStyle = const TextStyle() {
    _items.addAll(i);
  }

  List<TableRow> populateTableRows() {
    List<TableRow> rows = [
      TableRow(children: [
        Text(
          'Asset',
          style: _headerTxtStyle,
        ),
        Text(
          'Broker',
          style: _headerTxtStyle,
        ),
        Text(
          'Currency',
          style: _headerTxtStyle,
        ),
        Text(
          'Market Value',
          style: _headerTxtStyle,
          textAlign: TextAlign.right,
        ),
        financeValueText(mainCcy, _valueInMainCcy, positiveValueColor: Colors.blue),
        Text(
          'Profit',
          style: _headerTxtStyle,
          textAlign: TextAlign.right,
        ),
        financeValueText(mainCcy, _profitInMainCcy, positiveValueColor: Colors.blue),
      ])
    ];

    TableItem? greatParent;
    TableItem? parent;

    for (final item in _items) {
      if (item.level == Level.lvl1) {
        greatParent = item;
        // level 1 item is always populated
        rows.add(TableRow(children: [
          TableCell(
              verticalAlignment: TableCellVerticalAlignment.middle,
              child: Row(children: [
                IconButton(
                    onPressed: () => {print('expand')},
                    icon: item.expanded ? const Icon(Icons.expand_less) : const Icon(Icons.expand_more)),
                Text(item.name)
              ])),
          const Text(''), // lvl2 name
          const Text(''), // lvl3 name
          const Text(''), // Market value
          financeValueText(mainCcy, item.valueInMainCcy),
          const Text(''), // Profit
          financeValueText(mainCcy, item.profitInMainCcy),
        ]));
      } else {
        if (item.level == Level.lvl2) {
          parent = item;
          if (greatParent?.expanded ?? false) {
            rows.add(TableRow(children: [
              const Text(''), // lvl1 name
              TableCell(
                  verticalAlignment: TableCellVerticalAlignment.middle,
                  child: Row(children: [
                    IconButton(
                        onPressed: () => {print('expand')},
                        icon: item.expanded ? const Icon(Icons.expand_less) : const Icon(Icons.expand_more)),
                    Text(item.name)
                  ])),
              const Text(''), // lvl3 name
              const Text(''), // Market value
              TableCell(
                  verticalAlignment: TableCellVerticalAlignment.middle,
                  child: financeValueText(mainCcy, item.valueInMainCcy)),
              const Text(''), // Profit
              TableCell(
                  verticalAlignment: TableCellVerticalAlignment.middle,
                  child: financeValueText(mainCcy, item.profitInMainCcy)),
            ]));
          }
        } else if (greatParent?.expanded ?? false) {
          // leaf item
          if (parent?.expanded ?? false) {
            final name = item.name;
            final ccy = item.ccy;
            rows.add(TableRow(children: [
              const Text(''), // lvl1 name
              const Text(''), // lvl2 name
              Text(name),
              TableCell(verticalAlignment: TableCellVerticalAlignment.middle, child: financeValueText(ccy, item.value)),
              TableCell(
                  verticalAlignment: TableCellVerticalAlignment.middle,
                  child: financeValueText(mainCcy, item.valueInMainCcy)),
              TableCell(
                  verticalAlignment: TableCellVerticalAlignment.middle, child: financeValueText(ccy, item.profit)),
              TableCell(
                  verticalAlignment: TableCellVerticalAlignment.middle,
                  child: financeValueText(mainCcy, item.profitInMainCcy)),
            ]));
          }
        }
      }
    }

    return rows;
  }
}

class _OverviewWidgetState extends State<OverviewWidget> {
  Future<int>? _assets;
  TableItems? _items;

  OverviewGroup _lvl1 = groupByAsset;
  OverviewGroup _lvl2 = groupByBroker;
  OverviewGroup _lvl3 = groupByCcy;

  _OverviewWidgetState() {
    _assets = getAssets();
  }

  List<TableRow> _populateDataTable(BuildContext ctx, TextStyle headerTxtStyle, int? assetHandler) {
    if (_items == null) {
      if (assetHandler == null) return [];
      final ov = getOverview(assetHandler, mainCcy, _lvl1, _lvl2, _lvl3);
      _items = TableItems(headerTxtStyle, ov);
      urphFinFreeOverview(ov);
    }

    return _items?.populateTableRows() ?? [];
  }

  @override
  Widget build(BuildContext context) {
    return FutureBuilder(
        future: _assets,
        builder: (BuildContext ctx, AsyncSnapshot<int> snapshot) {
          if (snapshot.hasData) {
            const headerTxtStyle = TextStyle(fontWeight: FontWeight.bold);
            const headerTxtPadding = 40; //todo: compensate the size of expand button more wisely
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
                        //border: TableBorder.all(),
                        children: _populateDataTable(ctx, headerTxtStyle, snapshot.data))));
          } else {
            return const Center(child: AwaitWidget(caption: "Loading assets"));
          }
        });
  }
}
