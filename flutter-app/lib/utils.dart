// Copyright (c) 2019, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

import 'dart:io' show Platform;
import 'package:flutter/material.dart';
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


final Map<String, NumberFormat> _fmtByCcy = {
  'JPY': NumberFormat.currency(locale: 'en_US', name: 'JPY', symbol: '¥'),
  'CNY': NumberFormat.currency(locale: 'en_US', name: 'JPY', symbol: 'CN¥'),
  'USD': NumberFormat.currency(locale: 'en_US', name: 'USD', symbol: '\$'),
  'HKD': NumberFormat.currency(locale: 'en_US', name: 'HKD', symbol: 'HK\$'),
};

String formatCcy(String ccy, double num) {
  if (num == 0) return "";
  return (_fmtByCcy[ccy]?.format(num)) ?? '$ccy $num';
}

Widget financeValueText(BuildContext ctx, String ccy, double value) {
  return Text(formatCcy(ccy, value),
      textAlign: TextAlign.right,
      style: TextStyle(color: value < 0 ? Colors.red : Colors.black));
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