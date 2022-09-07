import 'dart:ffi';
import 'dart:math';

import 'package:ffi/ffi.dart';
import 'package:fl_chart/fl_chart.dart';
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
  List<TableItem> _items = [];
  final TextStyle _headerTxtStyle;
  late double _valueInMainCcy;
  late double _profitInMainCcy;
  late void Function(TableItem) _onExpandButtonPressed;
  static final _sumColors = [Colors.blue.shade900, Colors.blue.shade800, Colors.blue.shade400];
  static final _sumLossColors = [Colors.red.shade900, Colors.red.shade800, Colors.red.shade400];

  TableItems(this._headerTxtStyle, this._onExpandButtonPressed);

  void loadFromOverview(Pointer<Overview> overview) {
    _items = [];
    final ov = overview.ref;
    _valueInMainCcy = ov.value_sum_in_main_ccy;
    _profitInMainCcy = ov.profit_sum_in_main_ccy;

    for (int i = 0; i < ov.num; i++) {
      final containerContainer = ov.first[i];
      if (containerContainer.value_sum_in_main_ccy == 0) continue;
      _items.add(TableItem(Level.lvl1, containerContainer.name.toDartString(), containerContainer.value_sum_in_main_ccy,
          containerContainer.profit_sum_in_main_ccy));

      for (int j = 0; j < containerContainer.num; j++) {
        final container = containerContainer.containers[j];
        if (container.value_sum_in_main_ccy == 0) continue;
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

  void expandOrCollapseAll(bool expand) {
    for (final item in _items) {
      item.expanded = expand;
    }
  }

  List<TableRow> populateTableRows(OverviewGroup lvl1, OverviewGroup lvl2, OverviewGroup leaf) {
    List<TableRow> rows = [
      TableRow(children: [
        Text(
          getOverviewGroupName(lvl1),
          style: _headerTxtStyle,
        ),
        Text(
          getOverviewGroupName(lvl2),
          style: _headerTxtStyle,
        ),
        Text(
          getOverviewGroupName(leaf),
          style: _headerTxtStyle,
        ),
        Text(
          'Market Value',
          style: _headerTxtStyle,
          textAlign: TextAlign.right,
        ),
        financeValueText(mainCcy, _valueInMainCcy,
            positiveValueColor: _sumColors[0], negativeValueColor: _sumLossColors[0]),
        Text(
          'Profit',
          style: _headerTxtStyle,
          textAlign: TextAlign.right,
        ),
        financeValueText(mainCcy, _profitInMainCcy,
            positiveValueColor: _sumColors[0], negativeValueColor: _sumLossColors[0]),
      ])
    ];

    TableItem? greatParent;
    TableItem? parent;

    Widget textWithIconButton(final TableItem item) {
      return TableCell(
          verticalAlignment: TableCellVerticalAlignment.middle,
          child: Row(children: [
            IconButton(
                onPressed: () => _onExpandButtonPressed(item),
                icon: item.expanded ? const Icon(Icons.expand_less) : const Icon(Icons.expand_more)),
            Text(item.name)
          ]));
    }

    for (final item in _items) {
      if (item.level == Level.lvl1) {
        greatParent = item;
        // level 1 item is always populated
        rows.add(TableRow(children: [
          textWithIconButton(item),
          Container(), // lvl2 name
          Container(), // lvl3 name
          Container(), // Market value
          TableCell(
              verticalAlignment: TableCellVerticalAlignment.middle,
              child: financeValueText(mainCcy, item.valueInMainCcy,
                  positiveValueColor: _sumColors[1], negativeValueColor: _sumLossColors[1])),
          Container(), // Profit
          TableCell(
              verticalAlignment: TableCellVerticalAlignment.middle,
              child: financeValueText(mainCcy, item.profitInMainCcy,
                  positiveValueColor: _sumColors[1], negativeValueColor: _sumLossColors[1])),
        ]));
      } else {
        if (item.level == Level.lvl2) {
          parent = item;
          if (greatParent?.expanded ?? false) {
            rows.add(TableRow(children: [
              Container(), // lvl1 name
              textWithIconButton(item),
              Container(), // lvl3 name
              Container(), // Market value
              TableCell(
                  verticalAlignment: TableCellVerticalAlignment.middle,
                  child: financeValueText(mainCcy, item.valueInMainCcy,
                      positiveValueColor: _sumColors[2], negativeValueColor: _sumLossColors[2])),
              Container(), // Profit
              TableCell(
                  verticalAlignment: TableCellVerticalAlignment.middle,
                  child: financeValueText(mainCcy, item.profitInMainCcy,
                      positiveValueColor: _sumColors[2], negativeValueColor: _sumLossColors[2])),
            ]));
          }
        } else if (greatParent?.expanded ?? false) {
          // leaf item
          if (parent?.expanded ?? false) {
            final name = item.name;
            final ccy = item.ccy;
            rows.add(TableRow(children: [
              Container(), // lvl1 name
              Container(), // lvl2 name
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

typedef TitleAndValue = MapEntry<String, double>;

class _PieChartData {
  int touchedIndex = -1;
  final List<TitleAndValue> _items = [];
  final void Function(VoidCallback) _setState;

  _PieChartData(int assetHandler, Pointer<OverviewItemList> Function(int, String) loadData, this._setState) {
    final sumPtr = loadData(assetHandler, mainCcy);
    final sum = sumPtr.ref;
    for (int i = 0; i < sum.num; ++i) {
      final ovItem = sum.first[i];
      _items.add(TitleAndValue(ovItem.name.toDartString(), ovItem.value_in_main_ccy));
    }
    urphFinFreeOverviewItemList(sumPtr);
  }

  List<PieChartSectionData> _buildSections(BoxConstraints constrains) {
    List<PieChartSectionData> sections = [];

    const firstColor = 0xff0293ee;
    var color = firstColor;
    const colorStep = 8000;
    //0xfff8b250 - firstColor;

    final n = (min(constrains.maxHeight, constrains.maxWidth)) / 2;

    for (int i = 0; i < _items.length; ++i) {
      final isTouched = i == touchedIndex;
      final fontSize = isTouched ? 25.0 : 16.0;
      sections.add(PieChartSectionData(
          color: Color(color),
          title: _items[i].key,
          value: _items[i].value,
          radius: n,
          titleStyle: TextStyle(fontSize: fontSize, fontWeight: FontWeight.bold, color: const Color(0xffffffff))));
      color += colorStep;
    }

    return sections;
  }

  PieChartData populate(BoxConstraints constrains) {
    return PieChartData(
        pieTouchData: PieTouchData(touchCallback: (FlTouchEvent event, pieTouchResponse) {
          print('evnt $event');
          print('idx is ${pieTouchResponse?.touchedSection?.touchedSectionIndex}');
          if (event is FlPanDownEvent) {
            print('evnt d ${event.details}');
          }
          _setState(() {
            if (!event.isInterestedForInteractions ||
                pieTouchResponse == null ||
                pieTouchResponse.touchedSection == null) {
              touchedIndex = -1;
              return;
            }
            touchedIndex = pieTouchResponse.touchedSection!.touchedSectionIndex;
          });
        }),
        borderData: FlBorderData(
          show: false,
        ),
        sectionsSpace: 0,
        centerSpaceRadius: 0,
        sections: _buildSections(constrains));
  }
}

class _OverviewWidgetState extends State<OverviewWidget> {
  Future<int>? _assetsFuture;
  int assetHandler = 0;
  TableItems? _items;
  late _PieChartData _assetPieData;
  late _PieChartData _brokerPieData;
  bool expandAll = false;

  final List<OverviewGroup> _levels = [groupByAsset, groupByBroker, groupByCcy];

  @override
  void initState() {
    super.initState();
    _assetsFuture = getAssets();
    _assetsFuture?.then((value) => _onAssetsLoaded(value));
  }

  void _onAssetsLoaded(int handler) {
    assetHandler = handler;
    _assetPieData = _PieChartData(handler, getSumGroupByAsset, (callback) => setState(callback));
    _brokerPieData = _PieChartData(handler, getSumGroupByBroker, (callback) => setState(callback));
  }

  void _populateOverviewTable(int assetHandler) {
    final ov = getOverview(assetHandler, mainCcy, _levels[0], _levels[1], _levels[2]);
    _items?.loadFromOverview(ov);
    urphFinFreeOverview(ov);
  }

  List<TableRow> _populateDataTable(BuildContext ctx, TextStyle headerTxtStyle, int? handler) {
    if (handler == null) return [];
    //assetHandler = handler;

    if (_items == null) {
      _items = TableItems(
          headerTxtStyle,
          // called when the expand/collapse button is clicked
          (item) => setState(() => item.expanded = !item.expanded));
      _populateOverviewTable(assetHandler);
    }

    return _items?.populateTableRows(_levels[0], _levels[1], _levels[2]) ?? [];
  }

  @override
  Widget build(BuildContext context) {
    void updateGroupOrder(int oldIdx, int newIdx) {
      setState(() {
        final o = _levels[oldIdx];
        _levels.removeAt(oldIdx);
        _levels.insert(newIdx, o);
        expandAll = false;
        _populateOverviewTable(assetHandler);
      });
    }

    void setupFilter() {}

    void toggleExpansion() {
      setState(() {
        expandAll = !expandAll;
        _items?.expandOrCollapseAll(expandAll);
      });
    }

    Widget getGroupIcon(OverviewGroup group) {
      switch (group) {
        case groupByAsset:
          return const Icon(Icons.apartment_outlined);
        case groupByBroker:
          return const Icon(Icons.badge_outlined);
        default:
          return const Icon(Icons.attach_money_outlined);
      }
    }

    Widget buildOverviewTable(BuildContext ctx, int? asset) {
      const headerTxtStyle = TextStyle(fontWeight: FontWeight.bold);
      const headerPadding = 20;
      const headerPaddingWithButton = headerPadding + 20; //todo: compensate the size of expand button more wisely
      return Column(
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.start,
            crossAxisAlignment: CrossAxisAlignment.center,
            children: [
              ConstrainedBox(
                  constraints: const BoxConstraints(maxHeight: 60, maxWidth: 330),
                  child: ReorderableListView(
                    scrollDirection: Axis.horizontal,
                    onReorder: updateGroupOrder,
                    children: [
                      for (final level in _levels)
                        Padding(
                            key: ValueKey(level),
                            padding: const EdgeInsets.only(left: 2, right: 1, top: 2),
                            child: ElevatedButton.icon(
                                onPressed: setupFilter,
                                icon: getGroupIcon(level),
                                label: Text(getOverviewGroupName(level))))
                    ],
                  )),
              const SizedBox(
                  height: 60,
                  child: VerticalDivider(
                    width: 10,
                    indent: 7,
                    endIndent: 32,
                    thickness: 1,
                    color: Colors.grey,
                  )),
              ConstrainedBox(
                  constraints: const BoxConstraints(minHeight: 60),
                  child: Padding(
                      padding: const EdgeInsets.only(top: 2, bottom: 30),
                      child: ElevatedButton(
                          style: ElevatedButton.styleFrom(backgroundColor: Colors.indigo),
                          onPressed: toggleExpansion,
                          child: Text(expandAll ? 'Collapse All' : 'Expand All'))))
            ],
          ),
          Expanded(
              child: SingleChildScrollView(
            child: Padding(
                padding: const EdgeInsets.only(left: 20, right: 20),
                child: Table(
                    columnWidths: {
                      0: FixedColumnWidth(
                          getGroupTextSize(ctx, headerTxtStyle, _levels[0]).width + headerPaddingWithButton),
                      1: FixedColumnWidth(
                          getGroupTextSize(ctx, headerTxtStyle, _levels[1]).width + headerPaddingWithButton),
                      2: FixedColumnWidth(getGroupTextSize(ctx, headerTxtStyle, _levels[2]).width + headerPadding),
                      3: const FlexColumnWidth(1),
                      4: const FlexColumnWidth(1),
                      5: const FlexColumnWidth(1),
                      6: const FlexColumnWidth(1),
                    },
                    //border: TableBorder.all(),
                    children: _populateDataTable(ctx, headerTxtStyle, asset))),
          ))
        ],
      );
    }

    Widget buildCharts(BuildContext ctx) {
      Widget buildChart(_PieChartData pie) {
        return Expanded(
            flex: 1,
            child: Padding(
                padding: const EdgeInsets.all(10),
                child: LayoutBuilder(builder: (ctx, constrains) {
                  return PieChart(pie.populate(constrains));
                })));
      }

      return Column(
        children: [
          buildChart(_assetPieData),
          buildChart(_brokerPieData),
        ],
      );
    }

    return FutureBuilder(
        future: _assetsFuture,
        builder: (BuildContext ctx, AsyncSnapshot<int> snapshot) {
          if (snapshot.hasData) {
            return Row(children: [
              Expanded(flex: 1, child: buildOverviewTable(ctx, snapshot.data)),
              Expanded(flex: 1, child: buildCharts(ctx)),
            ]);
          } else {
            return const Center(child: AwaitWidget(caption: "Loading assets"));
          }
        });
  }
}
