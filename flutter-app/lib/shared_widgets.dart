import 'dart:ffi';

import 'package:flutter/material.dart';
import 'package:urph_fin/utils.dart';

Widget financeValueText(String ccy, double value,
    {Color positiveValueColor = Colors.black, Color negativeValueColor = Colors.red}) {
  return Text(formatCcy(ccy, value),
      textAlign: TextAlign.right, style: TextStyle(color: value < 0 ? negativeValueColor : positiveValueColor));
}

class AwaitWidget extends StatelessWidget {
  final String caption;
  const AwaitWidget({super.key, required this.caption});
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
              child: Directionality(textDirection: TextDirection.ltr, child: Text(caption))))
    ]);
  }
}

class QuoteWidget extends StatelessWidget {
  final String _symbol;
  final double _rate;
  const QuoteWidget(this._symbol, this._rate, {super.key});

  @override
  Widget build(BuildContext context) {
    return Container(child: Text(_symbol));
  }
}
