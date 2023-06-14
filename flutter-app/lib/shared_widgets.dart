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
  final double? progress;
  const AwaitWidget({super.key, required this.caption, this.progress});
  @override
  Widget build(BuildContext context) {
    return Column(mainAxisAlignment: MainAxisAlignment.center, children: [
      SizedBox(
        width: 60,
        height: 60,
        child: CircularProgressIndicator(
          value: progress,
        ),
      ),
      DefaultTextStyle(
          style: Theme.of(context).textTheme.displayMedium!,
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
    return Text(_symbol);
  }
}
