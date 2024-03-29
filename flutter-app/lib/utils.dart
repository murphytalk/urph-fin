// Copyright (c) 2019, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

import 'package:flutter/material.dart';
import 'dart:io' show Platform;
import 'package:intl/intl.dart' hide TextDirection;

String platformPath(String name, String path) {
  if (Platform.isLinux || Platform.isAndroid || Platform.isFuchsia) {
    if (Platform.isLinux && path == '') {
      path = "${Platform.environment['HOME']}/.local/lib64/";
    }
    return "${path}lib$name.so";
  }
  if (Platform.isMacOS) return "${path}lib$name.dylib";
  if (Platform.isWindows) return "$path$name.dll";
  throw Exception("Platform not implemented");
}

typedef OverviewGroup = int;
const int groupByAsset = 0;
const int groupByBroker = 1;
const int groupByCcy = 2;
const _overviewGroup = ["Asset", "Broker", "Currency"];
String getOverviewGroupName(OverviewGroup lvl) {
  return _overviewGroup[lvl];
}

Size getGroupTextSize(BuildContext ctx, TextStyle style, OverviewGroup group) {
  String txt = '';
  switch (group) {
    case groupByAsset:
      txt = 'Stock&ETF';
      break;
    case groupByBroker:
      txt = 'ABCDEFGHIJKLMNOPQ';
      break;
    case groupByCcy:
      txt = _overviewGroup[groupByCcy];
      break;
  }
  return calculateTextSize(text: txt, style: style, context: ctx);
}

final Map<String, NumberFormat> _fmtByCcy = {
  'JPY': NumberFormat.currency(locale: 'en_US', name: 'JPY', symbol: '¥'),
  'CNY': NumberFormat.currency(locale: 'en_US', name: 'CNY', symbol: 'CN¥'),
  'USD': NumberFormat.currency(locale: 'en_US', name: 'USD', symbol: '\$'),
  'HKD': NumberFormat.currency(locale: 'en_US', name: 'HKD', symbol: 'HK\$'),
};

String formatCcy(String ccy, double num) {
  if (num == 0) return "";
  return (_fmtByCcy[ccy]?.format(num)) ?? '$ccy $num';
}

final NumberFormat numFormatter = NumberFormat("###.00", "en_US");
String formatNum(double num) {
  return numFormatter.format(num);
}

final dtFormatter = DateFormat('yyyy-MM-dd');
String formatDate(DateTime dt) {
  return dtFormatter.format(dt);
}

String formatTimestamp(int dt) {
  return formatDate(DateTime.fromMillisecondsSinceEpoch(dt * 1000));
}

Size calculateTextSize({
  required String text,
  required TextStyle style,
  required BuildContext context,
}) {
  final double textScaleFactor = MediaQuery.of(context).textScaleFactor;
  final TextDirection textDirection = Directionality.of(context);

  final TextPainter textPainter = TextPainter(
    text: TextSpan(text: text, style: style),
    textDirection: textDirection,
    textScaleFactor: textScaleFactor,
  )..layout(minWidth: 0, maxWidth: double.infinity);

  return textPainter.size;
}
